/var/log/remotetrx {
    missingok
    notifempty
    weekly
    create 0644 svxlink daemon
    postrotate
        killall -HUP remotetrx
    endscript
}
