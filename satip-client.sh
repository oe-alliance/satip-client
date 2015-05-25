#!/bin/sh

if ! [ -x /usr/bin/satip-client ]; then
        exit 0
fi

. /etc/satip-client.conf

if [ "$DISABLED" = "yes" ]; then
        echo "satip-client is not configured. Please edit /etc/satip-client.conf !"
        exit 0
fi

case "$1" in
        start)
                if [ $SATIPSERVER0 ]; then
                        echo "Starting SAT>IP client on /dev/misc/vtuner0"
                        start-stop-daemon -S -b -p /var/run/satip-client0 -m -x /usr/bin/satip-client -- -s $SATIPSERVER0 -l$LOGLEVEL0 -t$FRONTENDTYPE0 -d /dev/misc/vtuner0 -w$WORKAROUND0 -y
                fi
                if [ $SATIPSERVER1 ]; then
                        echo "Starting SAT>IP client on /dev/misc/vtuner1"
                        start-stop-daemon -S -b -p /var/run/satip-client1 -m -x /usr/bin/satip-client -- -s $SATIPSERVER1 -l$LOGLEVEL1 -t$FRONTENDTYPE1 -d /dev/misc/vtuner1 -w$WORKAROUND1 -y
                fi
                if [ $SATIPSERVER2 ]; then
                        echo "Starting SAT>IP client on /dev/misc/vtuner2"
                        start-stop-daemon -S -b -p /var/run/satip-client2 -m -x /usr/bin/satip-client -- -s $SATIPSERVER2 -l$LOGLEVEL2 -t$FRONTENDTYPE2 -d /dev/misc/vtuner2 -w$WORKAROUND2 -y
                fi
                if [ $SATIPSERVER3 ]; then
                        echo "Starting SAT>IP client on /dev/misc/vtuner3"
                        start-stop-daemon -S -b -p /var/run/satip-client3 -m -x /usr/bin/satip-client -- -s $SATIPSERVER3 -l$LOGLEVEL3 -t$FRONTENDTYPE3 -d /dev/misc/vtuner3 -w$WORKAROUND3 -y
                fi
                ;;
        stop)
                start-stop-daemon -K -p /var/run/satip-client0 /usr/bin/satip-client
                start-stop-daemon -K -p /var/run/satip-client1 /usr/bin/satip-client
                start-stop-daemon -K -p /var/run/satip-client2 /usr/bin/satip-client
                start-stop-daemon -K -p /var/run/satip-client3 /usr/bin/satip-client
                ;;
        restart|reload)
                $0 stop
                $0 start
                ;;
        *)
                echo "Usage: $0 {start|stop|restart}"
                exit 1
                ;;
esac

exit 0
