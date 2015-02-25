/* $Id: messages.c,v 1.15 2001/01/13 18:18:11 rich Exp $ */


#include "config.h"
#include <stddef.h>      /* offsetof() */
#include <stdlib.h>      /* free() malloc() REALLOC() */
#include "diagnostic.h"  /* FAIL() LOG() */
#include "protocol.h"    /* Socket functions */
#include "messages.h"


/*
** Info on registered listeners.  #message# is the message for which #listener#
** is registered; #image# the message image.  Note that, since we provide no
** way to terminate listening for messages, we can simply expand the list by
** one every time a new listener is registered.
*/
typedef struct {
  MessageType message;
  const char *image;
  ListenFunction listener;
} ListenerInfo;
static ListenerInfo *listeners = NULL;
static unsigned listenerCount = 0;


/*
** A header sent with messages.  #version# is the NWS version and is presently
** ignored, but it could be used for compatibility.  #message# is the actual
** message.  #dataSize# is the number of bytes that accompany the message.
*/
typedef struct {
  unsigned int version;
  unsigned int message;
  unsigned int dataSize;
} MessageHeader;
static const DataDescriptor headerDescriptor[] =
{SIMPLE_MEMBER(UNSIGNED_INT_TYPE, 1, offsetof(MessageHeader, version)),
 SIMPLE_MEMBER(UNSIGNED_INT_TYPE, 1, offsetof(MessageHeader, message)),
 SIMPLE_MEMBER(UNSIGNED_INT_TYPE, 1, offsetof(MessageHeader, dataSize))};
#define headerDescriptorLength 3


/*
** Returns 1 or 0 depending on whether or not format conversion is required for
** data with the format described by the #howMany#-long array #description#.
*/
static int
ConversionRequired(const DataDescriptor *description,
                   size_t howMany) {
  int i;
  if(DataSize(description, howMany, HOST_FORMAT) !=
     DataSize(description, howMany, NETWORK_FORMAT)) {
    return 1;
  }
  for(i = 0; i < howMany; i++) {
    if(description[i].type == STRUCT_TYPE) {
      if(ConversionRequired(description[i].members, description[i].length))
        return 1;
    }
    else if(DifferentFormat(description[i].type))
      return 1;
  }
  return 0;
}


int
RecvData(Socket sd,
         void *data,
         const DataDescriptor *description,
         size_t howMany,
         double timeOut) {

  void *converted;
  int convertIt;
  void *destination;
  int recvResult;
  size_t totalSize = DataSize(description, howMany, NETWORK_FORMAT);

  converted = NULL;
  convertIt = ConversionRequired(description, howMany);

  if(convertIt) {
    converted = malloc(totalSize);
    if(converted == NULL)
      FAIL1("RecvData: memory allocation of %d bytes failed\n", totalSize);
    destination = converted;
  }
  else {
    destination = data;
  }

  recvResult = RecvBytes(sd, destination, totalSize, timeOut);

  if(DifferentOrder() || convertIt)
    ConvertData(data, destination, description, howMany, NETWORK_FORMAT);

  if(converted != NULL)
    free(converted);
  return recvResult;

}


int
RecvMessage(Socket sd,
            MessageType message,
            size_t *dataSize,
            double timeOut) {
  char garbage[1000];
  MessageHeader header;
  if(!RecvData(sd,
               (void *)&header,
               headerDescriptor,
               headerDescriptorLength,
               timeOut)) {
    FAIL("RecvMessage: no message received\n");
  }
  if(header.message != message) {
    while(header.dataSize > sizeof(garbage)) {
      (void)RecvBytes(sd, garbage, sizeof(garbage), timeOut);
      header.dataSize -= sizeof(garbage);
    }
    if(header.dataSize > 0) {
      (void)RecvBytes(sd, garbage, header.dataSize, timeOut);
    }
    FAIL1("RecvMessage: unexpected message %d received\n", header.message);
  }
  *dataSize = header.dataSize;
  return(1);
}

int
RecvMessageAndDatas(Socket sd,
                    MessageType message,
                    void *data1,
                    const DataDescriptor *description1,
                    size_t howMany1,
                    void *data2,
                    const DataDescriptor *description2,
                    size_t howMany2,
                    double timeOut) {
  char garbage[1000];
  MessageHeader header;

  if(!RecvData(sd,
               (void *)&header,
               headerDescriptor,
               headerDescriptorLength,
               timeOut)) {
    FAIL("RecvMessage: no message received\n");
  }

  if(header.message != message) {
    while(header.dataSize > sizeof(garbage)) {
      (void)RecvBytes(sd, garbage, sizeof(garbage), timeOut);
      header.dataSize -= sizeof(garbage);
    }
    if(header.dataSize > 0) {
      (void)RecvBytes(sd, garbage, header.dataSize, timeOut);
    }
    FAIL1("RecvMessage: unexpected message %d received\n", header.message);
  }

  if(data1 != NULL) {
    if(!RecvData(sd, data1, description1, howMany1, timeOut)) {
       FAIL("SendMessageAndData: data send failed\n");
    }
  }

  if(data2 != NULL) {
    if(!RecvData(sd, data2, description2, howMany2, timeOut)) {
       FAIL("SendMessageAndData: data send failed\n");
    }
  }

  return(1);
}

void
ListenForMessages(double timeOut) {

  MessageHeader header;
  int i;
  Socket sd;

  if(!IncomingRequest(timeOut, &sd))
    return;
  else if(!RecvData(sd,
                    (void *)&header,
                    headerDescriptor,
                    headerDescriptorLength,
                    PktTimeOut(sd))) {
    /*
    ** Likely a connection closed by the other side.  There doesn't seem to be
    ** any reliable way to detect this, and, for some reason, select() reports
    ** it as a connection ready for reading.
    */
    DROP_SOCKET(&sd);
    return;
  }

  for(i = 0; i < listenerCount; i++) {
    if(listeners[i].message == header.message) {
      LOG3("Received %s message from %s on %d\n",
           listeners[i].image, PeerName(sd), sd);
      listeners[i].listener(&sd, header.message, header.dataSize);
      break;
    }
  }

  if(i == listenerCount) {
    WARN3("Unknown message %d received from %s on %d\n",
          header.message, PeerName(sd), sd);
    DROP_SOCKET(&sd);
  }

}


void
RegisterListener(MessageType message,
                 const char *image,
                 ListenFunction listener) {
  listeners = REALLOC(listeners, (listenerCount + 1) * sizeof(ListenerInfo));
  listeners[listenerCount].message = message;
  listeners[listenerCount].image = image;
  listeners[listenerCount].listener = listener;
  listenerCount++;
}


int
SendData(Socket sd,
         const void *data,
         const DataDescriptor *description,
         size_t howMany,
         double timeOut) {

  void *converted;
  int sendResult;
  const void *source;
  size_t totalSize = DataSize(description, howMany, NETWORK_FORMAT);

  converted = NULL;

  if(DifferentOrder() || ConversionRequired(description, howMany)) {
    converted = malloc(totalSize);
    if(converted == NULL)
      FAIL1("SendData: memory allocation of %d bytes failed\n", totalSize);
    ConvertData(converted, data, description, howMany, HOST_FORMAT);
    source = converted;
  }
  else {
    source = data;
  }

  sendResult = SendBytes(sd, source, totalSize, timeOut);
  if(converted != NULL)
    free((void *)converted);
  return sendResult;

}


int
SendMessageAndDatas(Socket sd,
                    MessageType message,
                    const void *data1,
                    const DataDescriptor *description1,
                    size_t howMany1,
                    const void *data2,
                    const DataDescriptor *description2,
                    size_t howMany2,
                    double timeOut) {

  MessageHeader header;

  header.version = NWS_VERSION;
  header.message = message;
  header.dataSize = 0;
  if(data1 != NULL)
    header.dataSize += DataSize(description1, howMany1, NETWORK_FORMAT);
  if(data2 != NULL)
    header.dataSize += DataSize(description2, howMany2, NETWORK_FORMAT);

  if(!SendData(sd,
               &header,
               headerDescriptor,
               headerDescriptorLength,
               timeOut)) {
    FAIL("SendMessageAndDatas: header send failed \n");
  }
  if((data1 != NULL) &&
     !SendData(sd, data1, description1, howMany1, timeOut)) {
    FAIL("SendMessageAndDatas: data send failed\n");
  }
  if((data2 != NULL) &&
     !SendData(sd, data2, description2, howMany2, timeOut)) {
    FAIL("SendMessageAndDatas: data send failed\n");
  }
  return 1;

}
