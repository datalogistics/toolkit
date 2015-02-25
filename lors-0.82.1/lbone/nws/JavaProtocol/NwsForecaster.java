import java.io.*;
import java.net.*;


/**
 * The NwsForecaster class implements the protocol specific to NWS forecaster --
 * forecasting requests and method queries.
 */
public class NwsForecaster extends NwsHost {


  /** See the constructor for NwsHost. */
  public NwsForecaster(String hostName,
                       int hostPort,
                       boolean keepOpen) {
    super(hostName, hostPort, keepOpen);
  }


  /** See the constructor for NwsHost. */
  public NwsForecaster(String hostName,
                       int hostPort) {
    super(hostName, hostPort);
  }


  /** See the constructor for NwsHost. */
  public NwsForecaster(Socket s) {
    super(s);
  }


  /** A single forecast generated from a list of Measurements. */
  public static class Forecast {

    /** The forecast of the next value in the series. */
    public double forecast;
    /** An estimation of the accuracy of the forecast. */
    public double error;
    /** The index (see askMethods()) of the method that produced the forecast */
    public int methodUsed;

    /** Produces a Forecast with the specified field values. */
    public Forecast(double forecast,
                    double error,
                    int methodUsed) {
      this.forecast = forecast;
      this.error = error;
      this.methodUsed = methodUsed;
    }

    /** Produces a Forecast initialized by reading <i>stream</i>. */
    public Forecast(DataInputStream stream) throws Exception {
      forecast = stream.readDouble();
      error = stream.readDouble();
      methodUsed = stream.readInt();
    }

    /** Returns the fields of the Forecast converted to a byte array. */
    public byte[] toBytes() {
      byte[][] allBytes = {NwsMessage.toBytes(forecast),
                           NwsMessage.toBytes(error),
                           NwsMessage.toBytes(methodUsed)};
      return NwsMessage.concatenateBytes(allBytes);
    }

  }


  /** A set of forecast information produced from a single measurement. */
  public static class ForecastCollection {
    
    public static final int FORECAST_TYPE_COUNT = 2;
    public static final int MAE_FORECAST = 0;
    public static final int MSE_FORECAST = 1;

    /** The measurement from which the forecasts were produced. */
    public Measurement measurement;
    /**
     * A pair of forecasts based on the methods which produce the lowest mean
     * absolute error and mean square error.
     */
    public Forecast[] forecasts;

    /** Produces a ForecastCollection with the specified field values. */
    public ForecastCollection(Measurement measurement,
                              Forecast[] forecasts) {
      this.measurement = measurement;
      this.forecasts = forecasts;
    }

    /** Produces a ForecastCollection initialized by reading <i>stream</i>. */
    public ForecastCollection(DataInputStream stream) throws Exception {
      measurement = new Measurement(stream);
      forecasts = new Forecast[FORECAST_TYPE_COUNT];
      for(int i = 0; i < FORECAST_TYPE_COUNT; i++)
        forecasts[i] = new Forecast(stream);
    }

    /**
     * Returns the fields of the ForecastCollection converted to a byte array.
     */
    public byte[] toBytes() {
      byte[][] allBytes = {measurement.toBytes(),
                           forecasts[MAE_FORECAST].toBytes(),
                           forecasts[MSE_FORECAST].toBytes()};
      return NwsMessage.concatenateBytes(allBytes);
    }

  }


  /** A single measurement of resource availability. */
  public static class Measurement {

    /** The time (seconds since midnight 1/1/1970) the measurement was taken. */
    public double timeStamp;
    /** The observed availability at that time. */
    public double measurement;

    /** Produces a Measurement with the specified field values. */
    public Measurement(double timeStamp,
                       double measurement) {
      this.timeStamp = timeStamp;
      this.measurement = measurement;
    }

    /** Produces a Measurement initialized by reading <i>stream</i>. */
    public Measurement(DataInputStream stream) throws Exception {
      timeStamp = stream.readDouble();
      measurement = stream.readDouble();
    }

    /** Returns the fields of the Measurement converted to a byte array. */
    public byte[] toBytes() {
      byte[][] allBytes = {NwsMessage.toBytes(timeStamp),
                           NwsMessage.toBytes(measurement)};
      return NwsMessage.concatenateBytes(allBytes);
    }

  }


  /**
   * Returns a list of names of forecasting methods used by the forecaster.
   * The methodUsed field of the Forecast class can be used as an index into
   * this list to determine what method was used to generate the forecast.
   */
  public String[] askMethods() throws Exception {

    int methodCount;
    String methods;
    String[] returnValue;
    int tabPlace;

    messagePrelude();
    NwsMessage.send(connection, METHODS_ASK);
    methods = (String)NwsMessage.receive(connection, this);
    messagePostlude();

    methodCount = 1;
    for(tabPlace = methods.indexOf("\t");
        tabPlace != -1;
        tabPlace = methods.indexOf("\t", tabPlace + 1)) {
      methodCount++;
    }
    returnValue = new String[methodCount];

    for(int i = 0; i < methodCount - 1; i++) {
      tabPlace = methods.indexOf("\t");
      returnValue[i] = methods.substring(0, tabPlace);
      methods = methods.substring(tabPlace + 1);
    }
    returnValue[methodCount - 1] = methods;
    return returnValue;

  }


  /**
   * Requests that the forecaster generate a series of forecast collections
   * from <i>measurements</i>; the last <i>howMany</i> of this series are
   * returned.  <i>forecastName</i> identifies the measurement series;
   * forecasts are based both on the current series of measurements and any
   * measurements identified by the same name passed in prior calls.
   */
  public ForecastCollection[]
  getForecasts(String forecastName,
               Measurement[] measurements,
               int howMany) throws Exception {
    byte[][] allBytes = {null, null};
    ForecastCollection[] returnValue;
    allBytes[0] = new ForecastDataHeader(false,
                                         howMany,
                                         forecastName,
                                         measurements.length).toBytes();
    for(int i = 0; i < measurements.length; i++) {
      allBytes[1] = measurements[i].toBytes();
      allBytes[0] = NwsMessage.concatenateBytes(allBytes);
    }
    messagePrelude();
    NwsMessage.send(connection, FORE_DATA, allBytes[0]);
    returnValue = (ForecastCollection [])NwsMessage.receive(connection, this);
    messagePostlude();
    return returnValue;
  }


  /**
   * Requests that the forecaster generate a series of forecast collections
   * for the series <i>seriesName</i> which is held by the memory
   * <i>seriesMemory</i>; the last <i>howMany</i> are returned.
   */
  public ForecastCollection[]
  getForecasts(NwsMemory seriesMemory,
               String seriesName,
               int howMany) throws Exception {
    byte[][] allBytes = {NwsMessage.toBytes(1), null};
    ForecastCollection[] returnValue;
    allBytes[1] = new ForecastSeries(false,
                                     howMany,
                                     seriesName,
                                     seriesMemory.toString(),
                                     seriesName).toBytes();
    messagePrelude();
    NwsMessage.send
      (connection, FORE_SERIES, NwsMessage.concatenateBytes(allBytes));
    returnValue = (ForecastCollection [])NwsMessage.receive(connection, this);
    messagePostlude();
    return returnValue;
  }


  /** The header enclosed with FORE_DATA messages. */
  protected static class ForecastDataHeader {

    /** Will we ask for this series again?  Presently always set false. */
    public boolean moreToCome;
    /** The maximum number of ForecastCollections to return. */
    public int atMost;
    /** Identifies the forecast.  Supplied by the client. */
    public String forecastName;
    /** The number of measurements enclosed. */
    public int measurementCount;

    /** Produces a ForecastDataHeader with the specified field values. */
    public ForecastDataHeader(boolean moreToCome,
                              int atMost,
                              String forecastName,
                              int measurementCount) {
      this.moreToCome = moreToCome;
      this.atMost = atMost;
      this.forecastName = forecastName;
      this.measurementCount = measurementCount;
    }

    /** Produces a ForecastDataHeader initialized by reading <i>stream</i>. */
    public ForecastDataHeader(DataInputStream stream) throws Exception {
      moreToCome = stream.readInt() != 0;
      atMost = stream.readInt();
      forecastName = new CString(FORECAST_NAME_SIZE, stream).toString();
      measurementCount = stream.readInt();
    }

    /**
     * Returns the fields of the ForecastDataHeader converted to a byte array.
     */
    public byte[] toBytes() {
      byte[][] allBytes =
        {NwsMessage.toBytes(moreToCome ? 1 : 0),
         NwsMessage.toBytes(atMost),
         new CString(FORECAST_NAME_SIZE, forecastName).toBytes(),
         NwsMessage.toBytes(measurementCount)};
      return NwsMessage.concatenateBytes(allBytes);
    }

  }


  /** The header enclosed with FORE_SERIES messages. */
  protected static class ForecastSeries {

    /** Will we ask for this series again?  Presently always set false. */
    public boolean moreToCome;
    /** The maximum number of ForecastCollections to return. */
    public int atMost;
    /** Identifies the forecast.  Presently identical to the seriesName. */
    public String forecastName;
    /** The name of the memory holding the series in machine:port format. */
    public String memoryName;
    /** The name by which the Memory identifies the series. */
    public String seriesName;

    /** Produces a ForecastSeries with the specified field values. */
    public ForecastSeries(boolean moreToCome,
                          int atMost,
                          String forecastName,
                          String memoryName,
                          String seriesName) {
      this.moreToCome = moreToCome;
      this.atMost = atMost;
      this.forecastName = forecastName;
      this.memoryName = memoryName;
      this.seriesName = seriesName;
    }

    /** Produces a ForecastSeries initialized by reading <i>stream</i>. */
    public ForecastSeries(DataInputStream stream) throws Exception {
      moreToCome = stream.readInt() != 0;
      atMost = stream.readInt();
      forecastName = new CString(FORECAST_NAME_SIZE, stream).toString();
      memoryName = new CString(MAX_HOST_NAME, stream).toString();
      seriesName = new CString(STATE_NAME_SIZE, stream).toString();
    }

    /** Returns the fields of the ForecastSeries converted to a byte array. */
    public byte[] toBytes() {
      byte[][] allBytes =
        {NwsMessage.toBytes(moreToCome ? 1 : 0),
         NwsMessage.toBytes(atMost),
         new CString(FORECAST_NAME_SIZE, forecastName).toBytes(),
         new CString(MAX_HOST_NAME, memoryName).toBytes(),
         new CString(STATE_NAME_SIZE, forecastName).toBytes()};
      return NwsMessage.concatenateBytes(allBytes);
    }

  }


  protected static final int FORECAST_NAME_SIZE = 128;
  protected static final int STATE_NAME_SIZE = 128;

  protected static final int FORE_DATA = NwsMessage.FORECASTER_FIRST_MESSAGE;
  protected static final int FORE_FORECAST = FORE_DATA + 1;
  protected static final int FORE_SERIES = FORE_FORECAST + 1;
  protected static final int METHODS_ASK = FORE_SERIES + 1;
  protected static final int METHODS_TELL = METHODS_ASK + 1;

  protected static final int FORE_FAIL = NwsMessage.FORECASTER_LAST_MESSAGE;


  /** See NwsMessage. */
  public Object receive(int message,
                        DataInputStream stream,
                        int dataLength) throws Exception {
    if(message == METHODS_TELL) {
      return new CString(stream).toString();
    }
    else if(message == FORE_FORECAST) {
      new CString(FORECAST_NAME_SIZE, stream); /* Ignore the forecast name */
      ForecastCollection[] returnValue =
        new ForecastCollection[stream.readInt()];
      for(int i = 0; i < returnValue.length; i++)
        returnValue[i] = new ForecastCollection(stream);
      return returnValue;
    }
    else {
      return super.receive(message, stream, dataLength);
    }
  }


  /**
   * The main() method for this class is a small test program that takes a
   * forecaster spec, a memory spec, and a series name and displays 20
   * measurements and MSE forecasts for that series.
   */
  public static void main(String[] args) {
    final int MAE_FORECAST = NwsForecaster.ForecastCollection.MAE_FORECAST;
    final int MSE_FORECAST = NwsForecaster.ForecastCollection.MSE_FORECAST;
    try {
      NwsForecaster caster = new NwsForecaster(args[0], 8070);
      ForecastCollection[] forecasts =
        caster.getForecasts(new NwsMemory(args[1], 8050), args[2], 20);
      String[] methods = caster.askMethods();
      for(int i = 0; i < forecasts.length; i++) {
        ForecastCollection forecast = forecasts[i];
        System.out.println(
          (int)forecast.measurement.timeStamp + " " +
          forecast.measurement.measurement + " " +
          forecast.forecasts[MSE_FORECAST].forecast + " " +
          forecast.forecasts[MSE_FORECAST].error + " " +
          methods[forecast.forecasts[MSE_FORECAST].methodUsed].replace(' ', '_'));
      }
    } catch(Exception x) { System.err.println(x.toString()); }
  }


}
