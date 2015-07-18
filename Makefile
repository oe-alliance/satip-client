#
# Makefile for satip client
#
#
#Based on original vtuner source code from Google Code project.
#VTUNER_TYPE = VTUNER_TYPE_ORIGINAL
#
#Based on definitions found in the "usbtunerhelper" tool.
VTUNER_TYPE = VTUNER_TYPE_VUPLUS

CFLAGS += -Wall -g -D$(VTUNER_TYPE)

OBJ = satip_rtp.o satip_vtuner.o satip_config.o \
	satip_rtsp.o satip_main.o polltimer.o log.o
BIN = satip-client

$(BIN):  $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) -lrt -lpthread

clean:
	rm -f $(BIN) *.o *~
