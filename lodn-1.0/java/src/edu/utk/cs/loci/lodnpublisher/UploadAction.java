/* $Id: UploadAction.java,v 1.7 2008/05/24 22:25:54 linuxguy79 Exp $ */

package edu.utk.cs.loci.lodnpublisher;

import java.awt.event.ActionEvent;
import javax.swing.*;
import java.io.*;
import java.util.*;
import java.net.*;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.TrustManager;
import javax.net.ssl.KeyManager;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.HttpsURLConnection;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.security.cert.CertificateException;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchProviderException;
import java.security.UnrecoverableKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.KeyManagementException;
import edu.utk.cs.loci.exnode.*;
import edu.utk.cs.loci.lbone.*;
import edu.utk.cs.loci.ibp.*;

public class UploadAction extends AbstractAction {

	final static boolean DBG = false;

	private LoDNPublisher pub;

	private LboneServer servers[];
	static final int NBMAX_LBONESERVER = 10;
	static final int DURATION = 60 * 60 * 24 * 2; // 2 days

	private String lodnHost; // ex: "promise.sinrg.cs.utk.edu"
	private String cgibinpath; // ex: "/lodn/cgi-bin"
	private String encryption;
	private String port;
	private String depotsString;

	public UploadAction(LoDNPublisher pub, String lboneList, String lodnHost,
			String port, String encryption, String cgibinpath,
			String depotsString) {
		this(pub, lboneList, lodnHost, port, encryption, cgibinpath);

		this.depotsString = depotsString;
	}

	public UploadAction(LoDNPublisher pub, String lboneList, String lodnHost,
			String port, String encryption, String cgibinpath) {
		super("Upload");
		this.lodnHost = lodnHost;
		this.cgibinpath = cgibinpath;
		this.pub = pub;

		this.port = port;
		this.encryption = encryption;
		this.depotsString = null;

		int i = 0;
		servers = new LboneServer[NBMAX_LBONESERVER];

		StringTokenizer st = new StringTokenizer(lboneList);
		while (st.hasMoreTokens() && i < NBMAX_LBONESERVER) {
			String entry = st.nextToken().trim();
			StringTokenizer ste = new StringTokenizer(entry, ":");
			String hostname = ste.nextToken();
			int lport = Integer.parseInt(ste.nextToken());
			servers[i] = new LboneServer(hostname, lport);
			i++;
		}
	}

	public void actionPerformed(ActionEvent e) {

		final String inputFile = pub.getInputFile();
		final String outputFile = pub.getOutputFile();
		final String user = pub.getUser();
		final String pass = pub.getPass();
		final int connections = pub.getConnections();
		final int transferSize = pub.getTransferSize();
		final String location = pub.getLocation();
		final int copies = pub.getCopies(); // user instruction
		final int COPIES = 1; // upload only one copy from the user's system
		final int numDepots = pub.getDepots();
		final Exnode exnode = new Exnode();
		final File f = new File(inputFile);
		final long original_filesize = (long) f.length();
		final int mbPerDepot = (int) ((f.length() * COPIES) / numDepots / 1024
				/ 1024 + 1);

		final String functionName = pub.getExtraService();

		pub.exnode = exnode;

		if (functionName == null) {
			int okcancel = JOptionPane
					.showConfirmDialog(
							null,
							"Your file will be uploaded and stored UNENCRYPTED!\n"
									+ "For encrypted upload use the LoRS command line tools\n"
									+ "<http://loci.cs.utk.edu/lors>",
							"LoDN Warning", JOptionPane.OK_CANCEL_OPTION,
							JOptionPane.WARNING_MESSAGE);
			if (okcancel == JOptionPane.CANCEL_OPTION)
				return;
		}

		Thread t = new Thread(new Runnable() {
			public void run() {
				try {
					// 			LboneServer servers[]=new LboneServer[3];
					// 			servers[0]=new LboneServer("vertex.cs.utk.edu",6767);
					// 			servers[1]=new LboneServer("acre.sinrg.cs.utk.edu",6767);
					// 			servers[2]=new LboneServer("galapagos.cs.utk.edu",6767);

					boolean done = false;
					Depot[] depots = null;
					Depot[] requestedDepots = null;

					if (depotsString != null && !pub.getUseLbone()) {
						String depotNames[] = depotsString.split(":");

						if (depotNames.length < 1) {
							throw new Exception(
									"Unable to use specified deppts");
						}

						requestedDepots = new Depot[depotNames.length];

						for (int i = 0; i < depotNames.length; i++) {
							String depotComp[] = depotNames[i].split("/", 2);

							try {
								requestedDepots[i] = new Depot(depotComp[0],
										depotComp[1]);
							} catch (Exception e) {
								throw new Exception(
										"Unable to use specified depots");
							}
						}
					}

					for (int i = 0; i < servers.length && done == false; i++) {
						try {
							depots = servers[i].getDepots(numDepots,
									mbPerDepot, 0, DURATION, location,
									requestedDepots, 120);
							done = true;
						} catch (Exception ignored) {
							System.err.println("getDepots exception...");
						}
					}
					if (done == false) {
						throw (new Exception("Unable to contact L-Bone"));
					}

					DepotList list = new DepotList();
					list.add(depots);
					// randomizeNext: Avoid to start always on the same
					// depot especially if user always
					// use the same location.
					list.randomizeNext();

					System.out.println("Response from the L-Bone (" + numDepots
							+ " depots requested in " + location + " )");
					System.out.println(list.toString());

					exnode.write(inputFile, transferSize, connections,
							DURATION, COPIES, list, functionName);

					exnode.addMetadata(new StringMetadata("filename",
							outputFile.substring(
									outputFile.lastIndexOf('/') + 1, outputFile
											.length() - 4)));

					exnode.addMetadata(new DoubleMetadata("lorsversion", 0.82));

					exnode.addMetadata(new IntegerMetadata("number_of_copies",
							copies));

					// SIZE_ESTIMATE
					// -------------
					// "original_filesize" should be named "size_estimate"
					exnode.addMetadata(new IntegerMetadata("original_filesize",
							original_filesize));

					// STATUS
					// ------
					// NEW    (pink)  : has just been uploaded
					// GOOD   (green) : each block has more than x copies
					// DANGER (red)   : one (or more) block has only one copy available (still downloadable)
					// BROKEN (blue)  : hole(s) in the file (maybe downloadable later) 
					// DEAD   (black) : lot of holes (probably no chance to recover this file...)

					exnode.addMetadata(new StringMetadata("status", "NEW"));

					Metadata type = new ListMetadata("Type");
					type.add(new StringMetadata("Name", "logistical_file"));
					type.add(new StringMetadata("Version", "1.0"));
					exnode.addMetadata(type);

					Metadata tool = new ListMetadata("Authoring Tool");
					tool.add(new StringMetadata("Name", "LoDN"));
					tool.add(new StringMetadata("Version", "0.7"));
					exnode.addMetadata(tool);

					if (user == null) {
						FileOutputStream fos = new FileOutputStream(outputFile);
						fos.write(exnode.toXML().getBytes());
					} else {
						String serializedExnode = exnode.toXML();

						StringBuffer body = new StringBuffer();
						body
								.append("---------------------------56651459611944169561122610763\r\n");
						body
								.append("Content-Disposition: form-data; name=\"user\"\r\n");
						body.append("\r\n");
						body.append(user);
						body.append("\r\n");
						body
								.append("---------------------------56651459611944169561122610763\r\n");
						body
								.append("Content-Disposition: form-data; name=\"pass\"\r\n");
						body.append("\r\n");
						body.append(pass);
						body.append("\r\n");
						body
								.append("---------------------------56651459611944169561122610763\r\n");
						body
								.append("Content-Disposition: form-data; name=\"dir\"\r\n");
						body.append("\r\n");
						body.append(outputFile.substring(0, outputFile
								.lastIndexOf('/')));
						body.append("\r\n");
						body
								.append("---------------------------56651459611944169561122610763\r\n");
						body
								.append("Content-Disposition: form-data; name=\"filename\"; filename=\"");
						body.append(outputFile.substring(outputFile
								.lastIndexOf('/') + 1));
						body.append("\"\r\n");
						body
								.append("Content-Type: application/octet-stream\r\n");
						body.append("\r\n");
						body.append(serializedExnode);
						body.append("\r\n");
						body
								.append("---------------------------56651459611944169561122610763--");

						StringBuffer msg = new StringBuffer();
						msg.append("POST " + cgibinpath
								+ "/lodn_xnd_upload.cgi HTTP/1.1\r\n");
						msg.append("User-Agent: LoDN Publisher\r\n");
						msg.append("Host: " + lodnHost + ":" + port + "\r\n");
						msg
								.append("Content-Type: multipart/form-data; boundary=-------------------------56651459611944169561122610763\r\n");
						msg.append("Content-Length: " + body.length() + "\r\n");
						msg.append("\r\n");
						msg.append(body);

						System.out
								.println("\nUploading exNode on LoDN server..."
										+ lodnHost + ":" + port);

						Socket s = null;

						if (!encryption.equals("clear")) {
							try {

								/*KeyManager[]   km = null;
								            TrustManager[] tm = null; */

								/***--- The commented out code below until creation of the socketis for doing
								 *      self-signed certificates. Please leave it in case anyone else needs 
								 *      to do self-signed --***/

								/* Load the certificate chain and build the X509 certificate*/
								/*CertificateFactory cf = CertificateFactory.getInstance("X.509");
								BufferedInputStream bis = new BufferedInputStream(this.getClass().getResourceAsStream("/resources/server.crt"));
								X509Certificate cert = (X509Certificate)cf.generateCertificate(bis); */

								/* Creates an key store and puts the certificate into it */
								/* KeyStore ks = KeyStore.getInstance("JKS");
								ks.load(null, null);
								ks.setCertificateEntry("secretPwd", cert); */

								/* Creates an X509 Trust Manager and lands the certificate into it */
								/*TrustManagerFactory tmf = TrustManagerFactory.getInstance("SunX509");
								tmf.init(ks); */

								/* Creates a key factory and initializes it with the keystore */
								/*KeyManagerFactory kmf = KeyManagerFactory.getInstance("SunX509");
								kmf.init(ks, "secretPwd".toCharArray()); */

								/* Gets the key and trust managers */
								/*km = kmf.getKeyManagers();
								tm = tmf.getTrustManagers(); */

								/* Creates a ssl context object using either TLSv1 or SSLv3 and
								 * then has it use the certificate */
								/*SSLContext sslCTX = SSLContext.getInstance(encryption);
								  sslCTX.init(km, tm, null); */

								/* Creates a connected SSL socket to the server  */
								//s = sslCTX.getSocketFactory().createSocket(s, lodnHost, Integer.parseInt(port), true); 
								
								if(pub.badJVMversion) 
								{
									/* Creates a connected SSL socket to the server  */
									s = SSLSocketFactory.getDefault().createSocket(
											lodnHost, Integer.parseInt(port));
								}else
								{
									s = HttpsURLConnection
										.getDefaultSSLSocketFactory()
										.createSocket(lodnHost,
												Integer.parseInt(port));
								}
								
								String[] protocols = new String[1];
								protocols[0] = encryption;

								((SSLSocket) s).setEnabledProtocols(protocols);

							} catch (Exception e) {
								throw new Exception(
										"Unable to create SSL connection to LoDN.");
							}

							/* Creates a plain socket connection */
						} else {
							s = new Socket();
							s.connect(new InetSocketAddress(lodnHost, Integer
									.parseInt(port)));
						}

						/* Converts the message to the LoDN Server to a byte array */
						byte rawMsg[] = msg.toString().getBytes();

						
						/* Gets the output stream for sending to the server */
						OutputStream os = s.getOutputStream();

						/* Writes the exnode to the LoDN server */
						os.write(rawMsg);
						os.flush();

						


						System.out.println(" done.");

						System.out
								.println("Waiting response from LoDN server...");
						BufferedReader is = new BufferedReader(
								new InputStreamReader(s.getInputStream()));

						boolean moreData = true;
						boolean httpHdrDone = false;

						while (moreData) {
							String line = is.readLine();

							if (line == null) {
								moreData = false;
							} else if (line.length() == 0) {
								if (httpHdrDone) {
									moreData = false;
								}

								httpHdrDone = true;
							}

							System.out.println(line);
						}

						System.out.println(" done.");

						s.close();

						/* HERE THE UPLOAD SHOULD BE DEFINITELY COMPLETED */
						// Force the completion, just in case...
						exnode.progress.setDone();
						exnode.progress.fireProgressChangedEvent();
						exnode.progress.fireProgressDoneEvent();

					}

				} catch (Exception e) {
					e.printStackTrace();
					JOptionPane.showMessageDialog(null,
							"Unable to complete write operation: "
									+ e.getMessage(),
							"Error " + e.getMessage(),
							JOptionPane.ERROR_MESSAGE);
					exnode.setError(e);
					exnode.setCancel(true);
				}
			}
		});
		t.start();

		while (exnode.getProgress() == null) {
			try {
				this.wait(1000);
				System.out
						.println("Progress object not yet initialized...waiting...");
			} catch (Exception ignored) {
			}
		}

		//System.out.println("Initializing progress dialog...");	
		ProgressDialog progressDialog = new ProgressDialog(pub, f.length()
				* COPIES);
		progressDialog.setVisible(true);
	}
}
