SAT>IP Client FORK from: https://code.google.com/p/satip/

SAT>IP client for Linux attaching to vtuner driver of https://code.google.com/p/vtuner/

To be used e.g. with tvheadend, vdr, ...

A vtuner driver with some required tweaks is included in the repository.

First integration with tvheadend and XBMC successful.
Releases:

2014-03-22 - satip-0.0.2.tar.gz

    Fix: Keep satip configuration after TEARDOWN. VDR likely queries tuning status from driver and does not re-tune after restart
    Fix: PIDs are no longer swallowed when PID scanning and channel switching occurs in parallel 

2014-03-10 - satip-0.0.1.tar.gz

    This is a first release for packaging with openelec. 

Latest version - HEAD.zip

Check the change log for news.
Support:

Submit issues via the issue tracker or send email to project owner (captcha..).
Todo:

    Teardown on voltage off - done
    teardown when frontend is closed (?)
    RTP reception queuing
    flexible FE settings?
    real satip tuner status feedback
    ... 

SAT>IP Specification:

http://www.satip.info/resources

