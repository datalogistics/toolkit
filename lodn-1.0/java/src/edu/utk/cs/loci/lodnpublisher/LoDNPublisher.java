/* $Id: LoDNPublisher.java,v 1.6 2008/05/24 22:25:53 linuxguy79 Exp $ */

package edu.utk.cs.loci.lodnpublisher;

import java.io.*;
import java.util.Arrays;
import java.awt.event.*;

import javax.swing.*;

//import com.sun.tools.javac.code.Attribute.Array;

import edu.utk.cs.loci.exnode.*;

public class LoDNPublisher
{

    /* Activate some experimental features (aes, checksum, compress,...) when true */
    static final boolean EXPERIMENTAL_VERSION = false;

    JMenuBar menuBar;
    JMenu fileMenu;
    JMenu helpMenu;

    JLabel connectionsLabel;
    JLabel sizeLabel;
    JLabel locationLabel1;
    JLabel locationLabel2;
    JLabel copiesLabel;
    JLabel depotsLabel;
    JLabel lboneLabel;
    
    JTextField inputFileField;
    JTextField outputFileField;
    JTextField sizeField;
    JTextField locationField;
    JTextField depotsField;

    JCheckBox lboneCheckBox;
    
    JComboBox locationPrefixCombo;
    JComboBox connectionsCombo;
    JComboBox copiesCombo;

    JRadioButton kiloButton;
    JRadioButton megaButton;
    ButtonGroup group;

    JRadioButton dialupButton;
    JRadioButton dslcableButton;
    JRadioButton t3Button;
    JRadioButton highspeedButton;
    ButtonGroup cnxtypegroup;

    JButton inputBrowseButton;
    JButton uploadButton;
    JButton cancelButton;
    Exnode exnode = null;
    String homeDirectory;
    String user;
    String pass;
    String port;
    String encryption;
    String depotList;
      
    boolean useLBone = false;

    boolean badJVMversion = false; 	/* Bug fix for early version of Java 1.6  */ 
    
    static String dialupString = "Dial-Up/ISDN";
    static String dslcableString = "DSL/Cable/T1";
    static String t3String = "less than 100Mbps";
    static String highspeedString = "100Mbps and over";

    static String locationHintsString = "<html>e.g. zip=37996, state=TN, country=US, airport=TYS<html>";
    JLabel locationHintsLabel;

    File currentDirectory;

    Action inputBrowseAction;
    Action cancelAction;
    Action uploadAction;

    JRadioButton noneButton;
    JRadioButton xorButton;
    //JRadioButton desButton; // Not supported anymore by LoRS
    JRadioButton aesButton;
    JRadioButton md5Button;
    JRadioButton zipButton;
    ButtonGroup extraservicesgroup;

    static String noneString = "None";
    static String xorString = "XOR obfuscation";
    //    static String desString = "DES encryption"; // Not supported anymore by LoRS
    static String aesString = "AES encryption";
    static String md5String = "MD5 checksum";
    static String zipString = "Compression";

    static String extraservicesString = "<html>These services are currently under construction. Sorry for the inconvenience.<html>";
    JLabel extraservicesLabel;

    public LoDNPublisher( String homeDirectory )
    {
        this( homeDirectory, null, null, null, null, null, null, null, null);
    }
    
    public LoDNPublisher( String homeDirectory, String user, String pass, 
    		String lboneList, String host, String port, String encryption, 
    		String cgibinpath, String depotListString)
    {
        this.homeDirectory = homeDirectory;
        this.user = user;
        this.pass = pass;
        this.port = port;
        this.encryption = encryption;
        this.depotList  = depotListString;

        try
        {
            // Create a File object containing the canonical path of the
            // desired directory
            currentDirectory = new File( new File( "." ).getCanonicalPath() );
        }
        catch ( IOException e )
        {
        }

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

        locationHintsLabel = new JLabel( locationHintsString, JLabel.CENTER );
        locationHintsLabel.setBorder( BorderFactory.createTitledBorder( "Location Hints" ) );
        connectionsLabel = new JLabel( "Number of connections:" );
        connectionsLabel.setLabelFor( connectionsCombo );

        sizeField = new JTextField( "512" );
        sizeLabel = new JLabel( "Block size:" );
        sizeLabel.setLabelFor( sizeField );

        locationPrefixCombo = new JComboBox( new String[]
            { "zip= ", "state= ", "country= ", "airport= " } );
        locationPrefixCombo.setEditable( false );
        locationPrefixCombo.setSelectedIndex( 1 );

        locationLabel1 = new JLabel( "Location type:" );
        locationLabel1.setLabelFor( locationField );
        locationLabel2 = new JLabel( "Location code:" );
        locationLabel2.setLabelFor( locationPrefixCombo );

        copiesLabel = new JLabel( "Number of copies:" );
        copiesLabel.setLabelFor( copiesCombo );

        depotsLabel = new JLabel( "Number of depots:" );
        depotsLabel.setLabelFor( depotsField );
        
        lboneLabel = new JLabel("Use LBone:");
        lboneLabel.setLabelFor(lboneCheckBox);

        inputFileField = new JTextField();
        if ( EXPERIMENTAL_VERSION )
            inputFileField.setText( "/home/jp/testascii.txt" );

        outputFileField = new JTextField();
        outputFileField.setEditable( false );

        locationField = new JTextField();
        if ( EXPERIMENTAL_VERSION )
            locationField.setText( "tn" );

        depotsField = new JTextField( "20" );
        
        lboneCheckBox = new JCheckBox();
        
        this.useLBone = (depotListString == null);
        lboneCheckBox.setSelected(this.useLBone);
        
        lboneCheckBox.addItemListener(new ItemListener() 
        	{ 
        	
				public void itemStateChanged(ItemEvent e) 
				{
					if(depotList == null)
					{
						((JCheckBox)e.getItemSelectable()).setSelected(true);
					}
					
					useLBone = ((JCheckBox)e.getItemSelectable()).isSelected();
				} 
			});

		

        connectionsCombo = new JComboBox( new String[]
            { "1", "3", "6", "10" } );
        connectionsCombo.setEditable( true );
        connectionsCombo.setSelectedIndex( 0 );

        copiesCombo = new JComboBox( new String[]
            { "1", "2", "3", "4", "5" } );
        copiesCombo.setEditable( true );
        copiesCombo.setSelectedIndex( 2 );
        if ( EXPERIMENTAL_VERSION )
            copiesCombo.setSelectedIndex( 0 );

        kiloButton = new JRadioButton( "KB" );
        kiloButton.setSelected( true );
        megaButton = new JRadioButton( "MB" );
        group = new ButtonGroup();
        group.add( kiloButton );
        group.add( megaButton );

        inputBrowseAction = new InputBrowseAction( this );
        inputBrowseButton = new JButton( inputBrowseAction );
        inputBrowseButton.setMnemonic( KeyEvent.VK_B );

        uploadAction = new UploadAction( this, lboneList, host, port, encryption, cgibinpath, depotList);
        uploadButton = new JButton( uploadAction );
        uploadButton.setMnemonic( KeyEvent.VK_U );

        cancelAction = new CancelAction( this );
        cancelButton = new JButton( cancelAction );
        cancelButton.setMnemonic( KeyEvent.VK_X );

        // Extra services (encryptions, checksum,...) -----

        noneButton = new JRadioButton( noneString );
        noneButton.setSelected( true );

        xorButton = new JRadioButton( xorString );
        xorButton.setEnabled( true );
        //desButton = new JRadioButton(desString); // Not supported anymore by LoRS
        aesButton = new JRadioButton( aesString );
        aesButton.setEnabled( false );
        md5Button = new JRadioButton( md5String );
        md5Button.setEnabled( true );
        zipButton = new JRadioButton( zipString );
        zipButton.setEnabled( false );

        if ( EXPERIMENTAL_VERSION )
        {
            aesButton.setEnabled( true );
            zipButton.setEnabled( true );
        }

        extraservicesgroup = new ButtonGroup();
        extraservicesgroup.add( noneButton );
        extraservicesgroup.add( xorButton );
        //extraservicesgroup.add(desButton); // Not supported anymore by LoRS
        extraservicesgroup.add( aesButton );
        extraservicesgroup.add( md5Button );
        extraservicesgroup.add( zipButton );

        extraservicesLabel = new JLabel( extraservicesString, JLabel.CENTER );
        extraservicesLabel.setBorder( BorderFactory.createTitledBorder( "Note" ) );

        // Menu Bar configuration -------------------------
        fileMenu = new JMenu( "File" );
        fileMenu.setMnemonic( 'F' );

        JMenuItem openItem = fileMenu.add( inputBrowseAction );
        openItem.setText( "Open" );
        openItem.setMnemonic( 'O' );

        JMenuItem downloadItem = fileMenu.add( uploadAction );
        downloadItem.setMnemonic( 'U' );

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
        
        
        
        /***----  Sun Java 1.6 < 1.6.0_03 bug fix ---***/
        
        /* Gets the Java Vendor, version and the OS of the host */
		String vendor  = System.getProperty("java.vendor");
		String version = System.getProperty("java.version");
		String os      = System.getProperty("os.name");
		
		/* Detects if the vendor is Sun and the OS is Windows and the version
		 * is 1.6 */
		if(vendor.matches("^Sun.*") && os.matches(".*Windows.*") && 
		   version.startsWith("1.6.0") && !encryption.equals("clear"))
		{
			/* Gets the sub minor version of JVM */
			String subminorversion = version.replaceAll("1.6.0_*", "")
					                        .replaceAll("[^0-9]+.*", "");
					
			/* If sub minor version is less than 3 then the jvm could be bad */
			if(Integer.parseInt(subminorversion) < 3)
			{
				badJVMversion = true;
			}
		}
    }

    public void setInputFileName( String name )
    {
        inputFileField.setText( name );
    }

    // http://www.blooberry.com/indexdot/html/topics/urlencoding.htm
    public void setOutputFileName( String name )
    {
        outputFileField.setText( homeDirectory + "/" + cleanName( name )
            + ".xnd" );
    }

    // replace characters not supported (TODO)
    private String cleanName( String name )
    {
        final char[] chars = name.toCharArray();
        for ( int x = 0; x < chars.length; x++ )
        {
            final char c = chars[x];
            if ( ((c >= 'a') && (c <= 'z')) )
                continue; // a - z
            if ( ((c >= 'A') && (c <= 'Z')) )
                continue; // A - Z
            if ( ((c >= '0') && (c <= '9')) )
                continue; // 0 - 9
            if ( c == '-' )
                continue; // hyphen
            if ( c == '.' )
                continue; // dot
            chars[x] = '_'; // if not replaced by underscore
        }
        return String.valueOf( chars );
    }

    public String getInputFile()
    {
        return (inputFileField.getText());
    }

    public String getOutputFile()
    {
        if ( outputFileField.getText().equals( "" ) )
            setOutputFileName( new File( getInputFile() ).getName() );
        return (outputFileField.getText());
    }

    public String getLocation()
    {
        return (locationPrefixCombo.getSelectedItem() + locationField.getText());
    }

    public int getConnections()
    {
        return (Integer.parseInt( (String) connectionsCombo.getSelectedItem() ));
    }

    public int getTransferSize()
    {
        if ( kiloButton.isSelected() )
        {
            return (Integer.parseInt( sizeField.getText() ) * 1024);
        }
        else
        {
            return (Integer.parseInt( sizeField.getText() ) * 1024 * 1024);
        }
    }

    public int getCopies()
    {
        return (Integer.parseInt( (String) copiesCombo.getSelectedItem() ));
    }

    public int getDepots()
    {
        return (Integer.parseInt( depotsField.getText() ));
    }

    public Exnode getExnode()
    {
        return (exnode);
    }

    public String getUser()
    {
        return (user);
    }

    public String getPass()
    {
        return (pass);
    }

    public String getExtraService()
    {
        if ( noneButton.isSelected() )
            return null;
        if ( xorButton.isSelected() )
            return "xor_encrypt";
        //if(desButton.isSelected()) return "des_encrypt"; // Not supported anymore by LoRS
        if ( aesButton.isSelected() )
            return "aes_encrypt";
        if ( md5Button.isSelected() )
            return "checksum";
        if ( zipButton.isSelected() )
            return "zlib_compress";
        return null;
    }
    
    public boolean getUseLbone()
    {
    	return this.useLBone;
    }

    public static void main( String[] args )
    {
        // try {
        // 	UIManager.setLookAndFeel("com.jgoodies.plaf.plastic.PlasticLookAndFeel");
        // } catch(Exception e) {}

        LoDNPublisher pub = null;
        
        if ( args.length == 0 )
        {
            pub = new LoDNPublisher(System.getProperty( "user.home" ));
        }else
        {
        	String depotList = (args.length > 8) ? args[8] : null;
        	
            pub = new LoDNPublisher(
                args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], depotList);
        }

        System.out.println(pub);
        
        JFrame frame = new JFrame( "LoDN Publisher" );
        frame.addWindowListener( new WindowAdapter() );
        frame.setContentPane( new PublisherPanel( pub ) );
        frame.setJMenuBar( pub.menuBar );
        frame.pack();

        frame.setVisible( true );
    }
}
