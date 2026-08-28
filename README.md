# satipclient
SAT>IP Client FORK from: https://code.google.com/p/satip/

Supported options in /etc/vtuner.conf:
- tcpdata:1 - uses TCP instead of UDP for the connection with the satip server
- force_plts:1 - forces sending plts=on as part of the satip request
- fe:X - send fe=X as part of the satip request to force a specific adapter (useful on multiple satellite connections on different adapters)
- ipaddr - the ip address of the satip server
- port - the port of the satip server
- ca_pids:1 - also request the ECM/EMM pids of the active services, needed for CI(+) CAM descrambling (off by default, see below)
- ca_emm:0 - do not request the CAT and the EMM pids it announces (only used when ca_pids is on, default on)
- ca_caids:4AFC - only request ca pids of these caids, ';' separated hex values (only used when ca_pids is on, default: all caids)

## Descrambling with a CI(+) CAM (ca_pids)

A CI(+) CAM is a passive filter in the TS path: the transport stream is routed
through the CAM before it is demuxed, and the CAM never opens demux filters of
its own. Over SAT>IP only the pids that were explicitly requested are
delivered, so the ECM/EMM pids never arrive and the CAM cannot descramble
anything. On a local tuner the problem does not exist, because the full
transponder passes through the CAM.

With `ca_pids:1` satipclient looks for those pids itself. It taps the incoming
TS and follows the PAT, the CAT and the PMTs of the transponder, and adds the
ECM/EMM pids of the services that are currently being received to the RTSP pid
list.

A service counts as received when the kernel asked for one of its elementary
streams. Those pids stay joined for as long as a service is watched or
recorded, which is what makes the ecm pids follow a zap. The pmt pid on its own
does not qualify: a service scan opens a section filter on every pmt of the
transponder, which would otherwise mark every service as active at once.

To switch it on, append `ca_pids:1` to the tuner line in
`/etc/vtuner.conf`. That is all it takes, the other two options are
optional and only exist for the case below:

```
0=vtuner_type:satip_client,ipaddr:192.168.1.10,tuner_type:DVB-S,fe:1,ca_pids:1
```

On a multi CAS transponder every CA system announces its own ECM pid and the
CAT announces the EMM pids of all of them, which can add up to 10-20 extra
pids. Two options exist for servers that are short on hardware pid filters.
Every variant as a complete line:

```
# restrict to the caids the CAM handles, ';' separated hex
0=vtuner_type:satip_client,ipaddr:192.168.1.10,tuner_type:DVB-S,ca_pids:1,ca_caids:4AFC
0=vtuner_type:satip_client,ipaddr:192.168.1.10,tuner_type:DVB-S,ca_pids:1,ca_caids:4AFC;0500

# no CAT and no emm pids at all
0=vtuner_type:satip_client,ipaddr:192.168.1.10,tuner_type:DVB-S,ca_pids:1,ca_emm:0

# both
0=vtuner_type:satip_client,ipaddr:192.168.1.10,tuner_type:DVB-S,ca_pids:1,ca_caids:4AFC,ca_emm:0
```

(the `#` lines are comments for this README only, the parser does not
understand them, and it does not tolerate spaces around `:` or `,` either)

`ca_caids` filters ECM and EMM pids alike. The caids of a CAM hardly ever
change - a Kabelio CI+ module for example only needs 4AFC, while its Hotbird
transponders are simulcrypted with Panaccess and two Viaccess systems. Do not
guess the value though: a caid that is not on the transponder filters every ECM
pid away and descrambling stops, with nothing but a `filtered by ca_caids` note
in the log. Run without the option first and read the caids off the log.

`ca_emm:0` keeps the pid list minimal, but the card or module no longer gets
entitlement updates over the air, so it stops working whenever the provider
renews them. Prefer `ca_caids`, it saves the same pids without that risk.

The options have to be added by hand, the SAT>IP Client setup plugin does not
offer them (yet). It does keep them: it reads every attribute of a line into a
dictionary and writes all of them back, so saving in the plugin does not drop
them, and `;` is untouched because the plugin only splits on `,` and `:`.

### Testing it

The init script starts the daemon without any logging. To watch what happens,
stop it and run the client by hand:

```
/etc/init.d/satipclient stop
satipclient -l 3 2>/tmp/satip.log
```

It stays in the foreground and logs to stderr, so redirect it to a file if the
log is to be kept. Stop it with ctrl-c and put the daemon back with
`/etc/init.d/satipclient start`.

Log level 3 (info) prints the relevant lines and nothing else. The lines of the
parser carry a `[caN]` prefix, where N is the tuner (fe) number; the two lines
that show the actual RTSP request do not, they belong to the pid list itself:

```
[ca1] ecm/emm pid detection enabled (emm : on, caid filter : none, all CA systems)
[ca1] ts tap active, watching pids 0,1
[ca1] PAT version 12 -> 13
[ca1] PAT: 24 services, pmt pids 100,101,102,...
[ca1] CAT: emm pids 300,301
[ca1] PMT pid 100 (program 28006): ecm pids 500,501
[ca1] active services (pmt pids) : 100
[ca1] ---> ca pid set is now : 0,1,100,300,301,500,501
PLAY addpids : 1,300,301,500,501, delpids : - (now requested : 0,1,100,300,...)
```

The numbers above are made up, it is the shape of the output. The `SETUP pids`
and `PLAY addpids` lines are the proof that the pids really end up in the RTSP
request. Zap away from the service and the ECM pids have to show
up in `delpids` again. Once a minute a heartbeat line is written, so a log
without any ca pids can be told apart from a dead tap:

```
[ca1] heartbeat: 184320 ts packets, 942 sections, 0 crc errors, pat ok, 1 pmts known, ...
```

For more detail use `satipclient -l 4 -m 32`, which logs every single CA
descriptor with its caid (`-m 32` restricts the output to the ca category, plain
`-l 4` would also log the whole RTSP and vtuner traffic). `-y` logs to syslog
instead of stderr.

The same can be verified from the outside, without a CAM: check the `pids=`
parameter of the SETUP/PLAY requests in the SAT>IP server log (minisatip logs
them), or capture the RTSP session with `tcpdump -A -s0 port 554`.

## Building

```
autoreconf --install
./configure
make
```

To compile for e.g. VU Solo 4K (ARMv7 architecture):

```
./configure --with-boxtype=vusolo4k --host=arm-linux-gnueabihf
make
```

Make sure you have g++ and cross-compilation tools for `arm-linux-gnueabihf` installed 
(e.g. using `sudo apt-get install g++-arm-linux-gnueabihf`), otherwise you'll end up with a binary for your 
host's architecture.
