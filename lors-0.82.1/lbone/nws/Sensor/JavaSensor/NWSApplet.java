/*
 * "$Id: NWSApplet.java,v 1.1 1998/03/03 01:42:16 gbruno Exp $"
 */
import java.applet.*;
import java.io.*;
import java.net.*;
import java.util.Date;

public class NWSApplet extends Applet
{
	ServerSocket		javasock = null;
	int			java_server_port;

	static final int	DEFAULTSTARTPORT = 8060;
	static final int	DEFAULTEXPSIZE = (64*1024);
	static final int	DEFAULTMSGSIZE = 1024;

	public void init() {
		System.out.println("NWSApplet:init");

		String	param;

		param = getParameter("JavaServerPort");
		if (param == null) {
			java_server_port = DEFAULTSTARTPORT;
		} else {
			java_server_port = Integer.parseInt(param);
		}	

		try {
			/*
			 * open up a server-side socket and allow
			 * only 1 connection
			 */
			javasock = new ServerSocket(java_server_port, 1);

		} catch (Exception exception) {
			System.out.println("NWSApplet:init:exception: " +
				exception);
		}
	}

	public void start() {
		System.out.println("NWSApplet:start:" +
			"applet hostname:" + getCodeBase().getHost());

		String	param;
		int	remote_server_port;
		int	expsize;
		int	msgsize;
		int	ninight;

		param = getParameter("RemoteServerPort");
		if (param == null) {
			remote_server_port = DEFAULTSTARTPORT;
		} else {
			remote_server_port = Integer.parseInt(param);
		}

		param = getParameter("ExperimentSize");
		if (param == null) {
			expsize = DEFAULTEXPSIZE;
		} else {
			expsize = Integer.parseInt(param);
		}

		param = getParameter("MessageSize");
		if (param == null) {
			msgsize = DEFAULTMSGSIZE;
		} else {
			msgsize = Integer.parseInt(param);
		}

		param = getParameter("Period");
		if (param == null) {
			/*
			 * default to 5 seconds
			 */
			ninight = 5000;
		} else {
			ninight = Integer.parseInt(param) * 1000;
		}
		
		while (true) {
			try {
				double	latency_result;
				double	bandwidth_result;

				/*
				 * initiate experiments
				 */
				InitiateTCP_BW_Exp	initiate;

				initiate = new InitiateTCP_BW_Exp(
					getCodeBase().getHost(),
					remote_server_port,
					expsize, msgsize);
				latency_result = initiate.latency_test();
				bandwidth_result =
					initiate.bandwidth_test(expsize,
						msgsize);

				System.out.println("NWSApplet:start:" +
					"latency result " + latency_result);
				System.out.println("NWSApplet:start:" +
					"bandwidth result " + bandwidth_result);

				/*
				 * store the experiments
				 */
				initiate.StoreExperiments(
					getCodeBase().getHost(),
					remote_server_port,
					latency_result,
					bandwidth_result);

				/*
				 * create a clique on the
				 * remote network sensor
				 */
				Clique_Protocol	clique_msg = new
					Clique_Protocol(new
						Socket(getCodeBase().getHost(),
							remote_server_port));

				clique_msg.create(java_server_port,
					remote_server_port);
				clique_msg.close();

				/*
				 * wait for a connection
				 */
				Socket		clientsock = javasock.accept();
				TerminateTCP_BW_Exp	terminate;

				terminate = new
					TerminateTCP_BW_Exp(clientsock);	
				terminate.recv_experiment_params();
				terminate.latency_test();
				terminate.bandwidth_test();

				/*
				 * close down the connection
				 */
				clientsock.close();

			} catch(Exception exception) {
				System.out.println("NWSApplet:start:"
					+ "exception: " + exception);
			}

			try {
				/*
				 * if we complete ok or catch an exception,
				 * still need to take a snooze before trying
				 * the next experiment
				 */

				/*
				 * take a snooze
				 */
				java.lang.Thread.sleep(ninight);
			} catch(Exception exception) {
				System.out.println("NWSApplet:start:"
					+ "Thread exception: " + exception);
			}
		}
	}

	public void stop() {
		System.out.println("NWSApplet: stop");
	}

	public void destroy() {
		System.out.println("NWSApplet: destroy");

		try {
			javasock.close();
		} catch (IOException exception) {
			System.out.println("NWSApplet:destroy:exception: " +
				exception);
		}
	}
}

class TerminateTCP_BW_Exp
{
	Socket			sock;
	int			exp_size;
	int			buffer_size;
	int			msg_size;
	static final int	TCP_BW_EXP_REQ = 300;
	static final int	TCP_BW_EXP_REPLY = 300;

	TerminateTCP_BW_Exp(Socket clientsock) {
		sock = clientsock;
	}

	public void recv_experiment_params() throws Exception {
		System.out.println("TerminateTCP_BW_Exp:"
			+ "recv_experiment_params:called");

		Clique_Protocol		clique_msg;

		clique_msg = new Clique_Protocol(sock);
		clique_msg.recv_header();
		if (clique_msg.type != TCP_BW_EXP_REQ) {
			System.out.println(
				"TerminateTCP_BW_Exp:recv_experiment_params:"
				+ "unexpected message type:"
				+ clique_msg.type);	

			throw new Exception("TerminateTCP_BW_Exp:"
				+ "unexpected message type:"
				+ clique_msg.type);
		}

		/*
		 * now get the experiment parameters
		 */
		DataInputStream		in;

		in = new DataInputStream(sock.getInputStream());
		exp_size = in.readInt();
		buffer_size = in.readInt();	
		msg_size = in.readInt();	

		/*
		 * need to ack the receipt of the params
		 */
		clique_msg.send_header(TCP_BW_EXP_REPLY, 0, 0);

		DataOutputStream	out;

		out = new DataOutputStream(sock.getOutputStream());
		out.flush();
	}

	public void latency_test() throws Exception {
		System.out.println("TerminateTCP_BW_Exp:"
			+ "latency_test:called");

		DataInputStream		in;
		byte			dummy;
				
		in = new DataInputStream(sock.getInputStream());

		/*
		 * should only send us one byte
		 */
		dummy = in.readByte();

		/*
		 * send a response
		 */
		DataOutputStream	out;

		out = new DataOutputStream(sock.getOutputStream());
		out.writeByte(dummy);
		out.flush();
	}

	public void bandwidth_test() throws Exception {
		System.out.println("TerminateTCP_BW_Exp:"
			+ "bandwidth_test:started");

		DataInputStream		in;
		int			recv_bytes = 0;
		int			bytesread;
		byte			dummy[] = new byte[128];
		boolean			done = false;

		in = new DataInputStream(sock.getInputStream());

		while ((done == false) && (recv_bytes < exp_size)) {
			bytesread = in.read(dummy, 0, 128);

			if (bytesread == -1) {
				done = true;
			} else {
				recv_bytes += bytesread;
			}
		}

		if (recv_bytes != exp_size) {
			/*
			 * XXX - throw an exception?
			 */
			System.out.println(
				"TerminateTCP_BW_Exp:bandwidth_test:"
				+ "failed:" +
				"received " + recv_bytes + " bytes");
		}

		System.out.println("TerminateTCP_BW_Exp:"
			+ "bandwidth_test:completed");
	}
}

class InitiateTCP_BW_Exp
{
	Socket			sock;

	/*
	 * constants
	 */
	static final int	TCP_BW_EXP_REQ = 300;
	static final int	TCP_BW_EXP_REPLY = 300;
	static final int	STORE_STATE = 100;
	static final int	STATE_STORED = 101;
	static final int	STATE_ERR = 1100;
	static final int	JAVA_SENSOR = 100;

	InitiateTCP_BW_Exp(String endpoint, int port, int experiment_size,
			int msg_size) throws Exception {
		System.out.println("InitiateTCP_BW_Exp:begin test");

		Clique_Protocol		clique_msg;
		DataOutputStream	out;

		/*
		 * open up the socket
		 */
		sock = new Socket(endpoint, port);

		/*
		 * send the test parameters
		 */
		clique_msg = new Clique_Protocol(sock);
		clique_msg.send_header(TCP_BW_EXP_REQ, JAVA_SENSOR, 12);

		out = new DataOutputStream(sock.getOutputStream());

		/*
		 * experiment size
		 */
		out.writeInt(experiment_size);

		/*
		 * the size the endpoint should set its socket buffer size to
		 */
		out.writeInt(msg_size);

		/*
		 * the largest message this experment will send
		 */
		out.writeInt(msg_size);
		out.flush();

		clique_msg.recv_header();
		if (clique_msg.type != TCP_BW_EXP_REPLY) {
			System.out.println(
				"InitiateTCP_BW_Exp:"
				+ "unexpected message type:"
				+ clique_msg.type);	

			throw new Exception("InitiateTCP_BW_Exp:"
				+ "unexpected message type:" +
				clique_msg.type);
		}
	}

	public double latency_test() throws Exception {
		DataOutputStream	out;
		DataInputStream		in;
		long			start, finish;
		byte			dummy;
		
		out = new DataOutputStream(sock.getOutputStream());
		in = new DataInputStream(sock.getInputStream());

		start = System.currentTimeMillis();
		out.writeByte(69);
		out.flush();
		dummy = in.readByte();
		finish = System.currentTimeMillis();

		/*
		 * return the round trip time (in milliseconds)
		 */
		return((double)(finish-start));
	}

	public double bandwidth_test(int experiment_size,
			int msg_size) throws Exception {
		DataOutputStream	out;
		int			bytes_left = experiment_size;
		long			start, finish;
		double			retval = -1.0;
		byte			dummy[] = new byte[msg_size];
		
		out = new DataOutputStream(sock.getOutputStream());

		start = System.currentTimeMillis();
		while (bytes_left > 0) {
			if (bytes_left > msg_size) {
				out.write(dummy, 0, msg_size);
			} else {
				out.write(dummy, 0, bytes_left);
			}

			bytes_left -= msg_size;
		}
		out.flush();

		/*
		 * now wait for the other side to close the socket
		 */
		try {
			DataInputStream		in;

			in = new DataInputStream(sock.getInputStream());

			in.readByte();
		} catch (EOFException exception) {
			/*
			 * expect the other side to close down the
			 * connection to signal the end of the bandwidth
			 * experiment
			 */
			finish = System.currentTimeMillis();

			/*
			 * get a value in bits/seconds
			 */
			double		bits_per_second;

			bits_per_second = ((double)experiment_size * 8.0) /
				(((double)(finish-start)) / 1000.0);

			retval = bits_per_second / 1000000.0;
		}

		return(retval);
	}

	public void StoreExperiments(String endpoint, int port,
			double latency_result,
			double bandwidth_result) throws Exception {

		Socket			store_sock;
		DataOutputStream	out;
		Clique_Protocol		clique_msg;
		Date			timestamp = new Date();
		long			current_time;

		/*
		 * getTime() returns the number of "milliseconds" since
		 * the epoch.
		 */
		current_time = timestamp.getTime();
		current_time = current_time / 1000;

		/*
		 * open up the socket. need to open a new socket because
		 * the initial socket gets closed in the bandwidth
		 * experiment.
		 */
		store_sock = new Socket(endpoint, port);

		/*
		 * send the test parameters
		 */
		clique_msg = new Clique_Protocol(store_sock);
		clique_msg.send_header(STORE_STATE, JAVA_SENSOR, 24);

		out = new DataOutputStream(store_sock.getOutputStream());

		out.writeDouble((double)current_time);
		out.writeDouble(latency_result);
		out.writeDouble(bandwidth_result);
		out.flush();

		/*
		 * pick up the ack
		 */
		clique_msg.recv_header();
		switch (clique_msg.type) {
		case STATE_STORED:
			System.out.println("StoreExperiments:"
				+ "successful");
			break;

		case STATE_ERR:
			System.out.println("StoreExperiments:"
				+ "failed");
			break;

		default:
			throw new Exception("StoreExperiments:"
				+ "unexpected message type:" +
				clique_msg.type);
		}

		store_sock.close();
	}
}

class Clique_Protocol
{
	Socket			clique_sock = null;
	int			version;
	int			type;
	int			cmd;
	int			data_size;
	int			error;
	int			bytes_completed;

	static final int	VERSION			= 12;

	static final int	CLIQUE_TOKEN_FWD	= 400;
	static final int	CLIQUE_TOKEN_ACK	= 401;
	static final int	CLIQUE_KILL		= 402;
	static final int	CLIQUE_KILLED		= 403;
	static final int	CLIQUE_ACTIVATE		= 406;
	static final int	CLIQUE_ACTIVATED	= 407;

	static final int	JAVA_SENSOR		= 100;

	static final int	MAXCLIQUENAMESIZE	= 16;
	static final int	MAXTAGSIZE		= 16;

	static final String	CLIQUE_NAME = "java";
	static final String	TCP_BW_EXP = "bw.exp.rich";

	Clique_Protocol(Socket sock) {
		clique_sock = sock;
	}

	private int makeInt(byte addr[]) {
		int	address;

		address  = addr[0] & 0xFF;
		address |= ((addr[1] << 8) & 0xFF00);
		address |= ((addr[2] << 16) & 0xFF0000);
		address |= ((addr[3] << 24) & 0xFF000000);

		return(address);
	}

	public void send_header(int msg_type, int msg_cmd, int msg_data_size)
			throws Exception {

		DataOutputStream	out;

		out = new DataOutputStream(clique_sock.getOutputStream());
	
		out.writeInt(VERSION);		/* version */
		out.writeInt(msg_type);		/* type */
		out.writeInt(msg_cmd);		/* cmd */
		out.writeInt(msg_data_size);	/* data_size */
		out.writeInt(0);		/* error */
		out.writeInt(0);		/* bytes_completed */
	}

	public void recv_header() throws Exception {
		DataInputStream		in;

		in = new DataInputStream(clique_sock.getInputStream());

		version = in.readInt();
		type = in.readInt();
		cmd = in.readInt();
		data_size = in.readInt();
		error = in.readInt();
		bytes_completed = in.readInt();
	}

	public void create(int my_port, int remote_port) throws Exception {
		System.out.println("Clique_Protocol:"
			+ "create:called");

		byte	clique_name[] = new byte[MAXCLIQUENAMESIZE];
		byte	exp_tag[] = new byte[MAXTAGSIZE];
		int	msg_size;
		int	i;

		msg_size = MAXCLIQUENAMESIZE + 4 + 4 + MAXTAGSIZE + 8 + 8
			+ 8 + 8 + 4 + 16;

		send_header(CLIQUE_TOKEN_FWD, JAVA_SENSOR, msg_size);

		DataOutputStream	out;
		out = new DataOutputStream(clique_sock.getOutputStream());
		
		for (i = 0; i < CLIQUE_NAME.length(); ++i) {
			clique_name[i] = (byte)CLIQUE_NAME.charAt(i);
		}
		clique_name[i] = 0;
		out.write(clique_name, 0, MAXCLIQUENAMESIZE);

		out.writeInt(2);	/* count */
		out.writeInt(2);	/* hop_count */

		for (i = 0; i < TCP_BW_EXP.length(); ++i) {
			exp_tag[i] = (byte)TCP_BW_EXP.charAt(i);
		}
		exp_tag[i] = 0;
		out.write(exp_tag, 0, MAXTAGSIZE);

		/* instance */
		out.writeDouble(System.currentTimeMillis() / 1000);
		out.writeDouble(0.0);	/* seq_no */
		out.writeDouble(0.0);	/* period */
		out.writeDouble(31536000.0);	/* clique_timeout */
		out.writeInt(0);	/* leader */

		/*
		 * my ip address and port
		 */
		out.writeInt(makeInt(
			clique_sock.getLocalAddress().getAddress()));
		out.writeInt(my_port);

		/*
		 * remote side ip address and port.
		 *
		 * for some reason, java returns the remote address in
		 * host byte order, but the local address above is in
		 * network byte order.
		 */
		out.writeInt(clique_sock.getInetAddress().hashCode());
		out.writeInt(remote_port);

		/*
		 * pick up the ack
		 */
		recv_header();

		if (type != CLIQUE_TOKEN_ACK) {
			throw new Exception("Clique_Protocol:"
				+ "create:"
				+ "unexpected message type:" + type);
		}

		DataInputStream		in;
		byte			dummy[] = new byte[data_size];

		in = new DataInputStream(clique_sock.getInputStream());

		/*
		 * read in the rest of the message
		 */
		in.readFully(dummy, 0, data_size);
	}

	public void close() throws Exception {
		System.out.println("Clique_Protocol:"
			+ "close:called");

		clique_sock.close();
		clique_sock = null;
	}
}
