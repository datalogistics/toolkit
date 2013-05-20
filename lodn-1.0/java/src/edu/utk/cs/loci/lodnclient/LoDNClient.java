/* $Id: LoDNClient.java,v 1.4 2008/05/24 22:25:53 linuxguy79 Exp $ */

package edu.utk.cs.loci.lodnclient;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.FocusAdapter;
import java.awt.event.FocusEvent;
import java.awt.event.KeyEvent;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import javax.swing.Action;
import javax.swing.ButtonGroup;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JDialog;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JMenu;
import javax.swing.JMenuBar;
import javax.swing.JMenuItem;
import javax.swing.JOptionPane;
import javax.swing.JRadioButton;
import javax.swing.JTextField;

import org.lstore.client.FileSpecifier;
import org.lstore.client.FileSpecifierFactory;
import org.lstore.client.FileSpecifierUtil;
import org.lstore.client.LstoreApi;
import org.lstore.client.LstoreApiImplementation;
import org.lstore.util.Log;
import org.lstore.util.Password;

import edu.utk.cs.loci.exnode.DeserializeException;
import edu.utk.cs.loci.exnode.Exnode;
import edu.utk.cs.loci.exnode.StringMetadata;

public class LoDNClient
{

    JLabel connectionsLabel;
    JLabel sizeLabel;
    JTextField inputFileField;
    JTextField outputFileField;
    JTextField sizeField;
    JComboBox connectionsCombo;
    JRadioButton kiloButton;
    JRadioButton megaButton;
    ButtonGroup group;

    JRadioButton dialupButton;
    JRadioButton dslcableButton;
    JRadioButton t3Button;
    JRadioButton highspeedButton;
    ButtonGroup cnxtypegroup;

    final static String dialupString = "Dial-Up/ISDN";
    final static String dslcableString = "DSL/Cable/T1";
    final static String t3String = "less than 100Mbps";
    final static String highspeedString = "100Mbps and over";

    JButton inputBrowseButton;
    JButton outputBrowseButton;
    JButton downloadButton;
    JButton cancelButton;
    JButton executeButton;
    Action cancelAction;
    Action downloadAction;
    Action executeAction;
    Action inputBrowseAction;
    Action outputBrowseAction;
    JMenuBar menuBar;
    JMenu fileMenu;
    JMenu helpMenu;
    JCheckBox closeAfterFinishedCheck;

    Exnode[] exnodes = null;
    
    private String lStoreLoginId = "";
    private String lStorePassword = "";
    private String downloadedBaseName = ""; 
    private boolean closeAfterTransfer = false;

    public static final String LOGIN_ARGUMENT = "login:";
    public static final String PASSWORD_ARGUMENT = "password:";
    public static final String FILE_ARGUMENT = "file:";
    public static final String FILE_NAME_ARGUMENT = "downloaded_file_name:";
    public static final String LSTORE_PREFIX = "lstore:";
    public static final String LSTORE_URI_PREFIX = LSTORE_PREFIX + "//";
    
    public LoDNClient()
    {
        connectionsLabel = new JLabel( "Number of connections:" );
        connectionsLabel.setLabelFor( connectionsCombo );

        sizeLabel = new JLabel( "Transfer block size:" );
        sizeLabel.setLabelFor( sizeField );

        inputFileField = new JTextField();
        inputFileField.setEnabled( false );
        inputFileField.addFocusListener( new FocusAdapter()
            {
                public void focusLost( FocusEvent e )
                {
                    if ( !inputFileField.isEnabled() )
                    {
                        return;
                    }

                    if ( retrieveExnode( inputFileField.getText() ) )
                    {
                        outputFileField.setText( getDefaultOutputFile("") );
                    }
                    else
                    {
                        inputFileField.setText( "" );
                    }
                }
            } );
        closeAfterFinishedCheck = new JCheckBox("Close after completing download", isCloseAfterTransfer());
        closeAfterFinishedCheck.addActionListener(new ActionListener() {
		
			public void actionPerformed(ActionEvent e) {
				setCloseAfterTransfer(closeAfterFinishedCheck.isSelected());
			}
		});

        outputFileField = new JTextField();

        dialupButton = new JRadioButton( dialupString );
        dialupButton.setActionCommand( dialupString );
        dialupButton.setMnemonic( KeyEvent.VK_I );
        dialupButton.setSelected( true );

        dslcableButton = new JRadioButton( dslcableString );
        dslcableButton.setActionCommand( dslcableString );
        dslcableButton.setMnemonic( KeyEvent.VK_S );

        t3Button = new JRadioButton( t3String );
        t3Button.setActionCommand( t3String );
        t3Button.setMnemonic( KeyEvent.VK_L );

        highspeedButton = new JRadioButton( highspeedString );
        highspeedButton.setActionCommand( highspeedString );
        highspeedButton.setMnemonic( KeyEvent.VK_1 );

        cnxtypegroup = new ButtonGroup();
        cnxtypegroup.add( dialupButton );
        cnxtypegroup.add( dslcableButton );
        cnxtypegroup.add( t3Button );
        cnxtypegroup.add( highspeedButton );

        CnxTypeAction cnxta = new CnxTypeAction( this );
        dialupButton.addActionListener( cnxta );
        dslcableButton.addActionListener( cnxta );
        t3Button.addActionListener( cnxta );
        highspeedButton.addActionListener( cnxta );

        sizeField = new JTextField( "512" );

        connectionsCombo = new JComboBox( new String[]
            { "1", "3", "6", "10", "15", "20" } );
        connectionsCombo.setEditable( true );
        connectionsCombo.setSelectedIndex( 5 );

        kiloButton = new JRadioButton( "KB" );
        kiloButton.setSelected( true );

        megaButton = new JRadioButton( "MB" );

        group = new ButtonGroup();
        group.add( kiloButton );
        group.add( megaButton );

        cancelAction = new CancelAction( this );
        downloadAction = new DownloadAction( this );
        executeAction = new ExecuteAction( this );
        inputBrowseAction = new InputBrowseAction( this );
        inputBrowseAction.setEnabled( false );
        outputBrowseAction = new OutputBrowseAction( this );

        inputBrowseButton = new JButton( inputBrowseAction );
        outputBrowseButton = new JButton( outputBrowseAction );
        outputBrowseButton.setMnemonic( KeyEvent.VK_B );

        downloadButton = new JButton( downloadAction );
        downloadButton.setMnemonic( KeyEvent.VK_D );

        cancelButton = new JButton( cancelAction );
        cancelButton.setMnemonic( KeyEvent.VK_X );

        executeButton = new JButton( executeAction );
        executeButton.setMnemonic( KeyEvent.VK_E );

        fileMenu = new JMenu( "File" );
        fileMenu.setMnemonic( 'F' );

        JMenuItem openItem = fileMenu.add( inputBrowseAction );
        openItem.setText( "Open" );
        openItem.setMnemonic( 'O' );

        JMenuItem downloadItem = fileMenu.add( downloadAction );
        downloadItem.setMnemonic( 'D' );

        JMenuItem executeItem = fileMenu.add( executeAction );
        executeItem.setMnemonic( 'E' );

        fileMenu.addSeparator();

        JMenuItem exitItem = fileMenu.add( cancelAction );
        exitItem.setText( "Exit" );
        exitItem.setMnemonic( 'X' );

        helpMenu = new JMenu( "Help" );
        helpMenu.setMnemonic( 'H' );

        JMenuItem contentsItem = helpMenu.add( new ContentsAction() );
        contentsItem.setMnemonic( 'C' );

        JMenuItem aboutItem = helpMenu.add( new AboutAction() );
        aboutItem.setMnemonic( 'A' );

        menuBar = new JMenuBar();
        menuBar.add( fileMenu );
        menuBar.add( helpMenu );
    }

    boolean retrieveExnode(String uri)
    {
        return retrieveExnodes(uri, 0);
    }

    private Exnode ExtractExnodeFromlStore(FileSpecifier fileSpec) throws IOException, DeserializeException {
    	try {
    		Log.setLogLevel(Log.Level.TRACE);
    	} catch (Exception e) {
    		e.printStackTrace();
    	}
    	if (fileSpec.isRemote()) {
    		LstoreApi conn = new LstoreApiImplementation(FileSpecifierUtil.getRemoteAddress(fileSpec));
    		      if (conn.initialLogin(null, getLStoreLoginId(), getLStorePassword(), "127.0.0.1", null, null)) {
    		    		String exnodeAsString = conn.getExnode(fileSpec.getPath(), fileSpec.getFilename(), getLStoreLoginId());
    		    		return Exnode.fromXML(exnodeAsString);
    		       } else {
    		           System.err.println("login to lstore failed");
    		       }
    	}
    	return null;
    }
    
    private Exnode extractExnodeFromUri(String uri) throws DeserializeException, IOException {
    	if (isLstoreUri(uri)) {
    		return ExtractExnodeFromlStore(FileSpecifierFactory.getFileSpecifier(uri.substring(LSTORE_URI_PREFIX.length()), getLStoreLoginId()));
    	} else {
    		return Exnode.fromURI( uri );
    	}
    }
    
    private boolean isLstoreUri(String uri) {
    	return uri.startsWith(LSTORE_URI_PREFIX);
    }

    boolean retrieveExnodes( String uri, int i )
    {
        JDialog startupDialog = new StartupDialog( i );
        try
        {
            startupDialog.setVisible( true );
            exnodes[i] = extractExnodeFromUri(removeDoubleQuotes(uri));
            startupDialog.setVisible( false );
            return (exnodes[i] != null);
        }
        catch ( Exception e )
        {
        	e.printStackTrace();
            startupDialog.setVisible( false );
            JOptionPane.showMessageDialog( null, "Unable to deserialize: "
                + e.getMessage(), "Error", JOptionPane.ERROR_MESSAGE );
            return (false);
        }
    }
    
    private String removeDoubleQuotes (String uri) {
    	return uri.replaceAll("\"(" +LSTORE_URI_PREFIX + "..*)\"", "$1"); 
    }

    String getDefaultOutputFile( Exnode exnode )
	{
		return getDefaultOutputFile(exnode, "");
	}

	String getDefaultOutputFile( Exnode exnode, String defaultName )
    {
		String out = null;
        StringMetadata filenameMetadata = (StringMetadata) exnode.getMetadata( "filename" );
        String filename;
        try
        {
            filename = filenameMetadata.getString();
        }
        catch ( NullPointerException e )
        {
            filename = defaultName;
        }
        String homeDirectory = getHomeDirectory();
        String separator = File.separator;
        out = generateUniqeName(new String (homeDirectory + separator + filename));
        return out;
    }

    private String generateUniqeName(String fileName) {
    	File f = new File (fileName);
		int counter = 1;
		String out = fileName;
		if (!f.exists())
			return fileName;
		else {
			out = new String (fileName + "_" + counter);
			f = new File (out);
			while (f.exists()) {
				counter++;
				out = new String (fileName + "_" + counter);
				f = new File (out);
			}
		}
		return out;
	}

	private String getHomeDirectory() {
    	String out = null;
	String osName = System.getProperty( "os.name" );
	String windowsRegex = "^[Ww]indows..*$";
	String macRegex = "^[Mm]ac..*$";
	if (osName.matches(windowsRegex))
		out = new String (System.getProperty("user.home") + File.separator + "Desktop");
	else if (osName.matches(macRegex))
		out = new String (System.getProperty("user.home") + File.separator + "Downloads");
	else // We assume anything else is some sort of Linux/Unix
		out = System.getProperty("user.home");
	return out;
	}

	String getDefaultOutputFile()
	{
		return getDefaultOutputFile("");
	}

	String getDefaultOutputFile(String defaultName)
    {
        return getDefaultOutputFile( exnodes[0], defaultName );
    }

    /*
     * Arguments:
     * login:loginId
     * password:password_in_clear_text
     * file:file_url_descriptor
     */
    public static void main( String[] args )
    {
//        try {
//        	UIManager.setLookAndFeel("com.jgoodies.plaf.plastic.PlasticLookAndFeel");
//        } catch(Exception e) {
//        }

        LoDNClient client = new LoDNClient();

        if ( args.length == 0 )
        {
            client.inputFileField.setEnabled( true );
            client.inputBrowseAction.setEnabled( true );
            client.exnodes = new Exnode[1];
        }
        else
        {
        	System.out.println("Arguments are:");
        	for (String arg: args) 
        		System.out.println(arg);
        	List<String> files = parseArguments(args, client);
        	client.exnodes = new Exnode[files.size()];
            setExnodes(client, files);
            updateGUI(client, files);

        }

        JFrame frame = new JFrame( "LoDN Client" );
        frame.addWindowListener( new WindowAdapter() );
        frame.setContentPane( new ClientPanel( client ) );
        frame.setJMenuBar( client.menuBar );
        frame.pack();

        client.downloadButton.requestFocus();

        frame.setVisible( true );
    }

	private static void updateGUI(LoDNClient client, List<String> files) {
		if ( files.size() == 1 )
		{
			String theFile = client.removeDoubleQuotes(files.get(0));
		    client.inputFileField.setText(theFile);
		    if ( client.retrieveExnode(files.get(0)) == false )
		    {
		        System.exit( 0 );
		    }
		    client.outputFileField.setText( client.getDefaultOutputFile(client.getBasename(theFile, client)) );
		}
		else
		{
		    client.inputFileField.setText( "-Multiple selection can not be displayed-" );
		    String homeDirectory = System.getProperty( "user.home" );
		    String separator = System.getProperty( "file.separator" );
		    client.outputFileField.setText( homeDirectory + separator );
		    client.outputFileField.setEnabled( false );
		    client.outputBrowseAction.setEnabled( false );
		    client.executeAction.setEnabled( false );
		}
	}

	private String getBasename(String aFile, LoDNClient client) {
		String out;
		if (client.getDownloadedBaseName().equals("")) {
			if (isLstoreUri(aFile)) {
				out = FileSpecifierFactory.getFileSpecifier(aFile.substring(LSTORE_URI_PREFIX.length()), getLStoreLoginId()).getFilename();
			} else {
				out = "";
			}
		} else {
			out = client.getDownloadedBaseName();
		}
		return out;
	}
	
	private static List<String> parseArguments(String [] args, LoDNClient client) {
		List<String> files = new ArrayList<String>();
		for (String arg: args) {
			if (arg.startsWith(LoDNClient.LOGIN_ARGUMENT)) {
				client.setLStoreLoginId(arg.substring(LoDNClient.LOGIN_ARGUMENT.length()));
			} else if (arg.startsWith(LoDNClient.PASSWORD_ARGUMENT)) {
				client.setLStorePassword(Password.getMD5hashPassword(arg.substring(LoDNClient.PASSWORD_ARGUMENT.length())));
			} else if (arg.startsWith(LoDNClient.FILE_NAME_ARGUMENT)) {
				client.setDownloadedBaseName(arg.substring(LoDNClient.FILE_NAME_ARGUMENT.length()));
			} else if (arg.startsWith(LoDNClient.FILE_ARGUMENT)) {
				files.add(arg.substring(LoDNClient.FILE_ARGUMENT.length()));
			} else {
				// TODO throw an exception or something
			}
		}
		return files;
	}

	private static void setExnodes(LoDNClient client, List<String> files) {
		for ( int i = 0; i < files.size(); i++ )
        {
            if ( client.retrieveExnodes( files.get(i), i ) == false )
            {
                System.exit( 0 );
            }
        }
	}

	public void setLStoreLoginId(String lStoreLoginId) {
		this.lStoreLoginId = lStoreLoginId;
	}

	public String getLStoreLoginId() {
		return lStoreLoginId;
	}

	public void setLStorePassword(String lStorePassword) {
		this.lStorePassword = lStorePassword;
	}

	public String getLStorePassword() {
		return lStorePassword;
	}

	public String getDownloadedBaseName() {
		return downloadedBaseName;
	}

	public void setDownloadedBaseName(String downloadedBaseName) {
		this.downloadedBaseName = downloadedBaseName;
	}

	public boolean isCloseAfterTransfer() {
		return closeAfterTransfer;
	}

	public void setCloseAfterTransfer(boolean closeAfterTransfer) {
		this.closeAfterTransfer = closeAfterTransfer;
	}
}