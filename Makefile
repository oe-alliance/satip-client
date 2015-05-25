#
# Makefile for satip client
#
#
VTUNER_TYPE = VTUNER_TYPE_ORIGINAL

CFLAGS += -Wall -g -D$(VTUNER_TYPE)

OBJ = satip_rtp.o satip_vtuner.o satip_config.o \
	satip_rtsp.o satip_main.o polltimer.o log.o
BIN = satip-client

$(BIN):  $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) -lrt -lpthread

clean:
	rm -f $(BIN) *.o *~
