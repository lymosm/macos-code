1. install
chmod +x install_timesyncd.sh
sudo ./install_timesyncd.sh /绝对路径/LymosTimeSyncd.c

2. uninstall
chmod +x uninstall_timesyncd.sh
sudo ./uninstall_timesyncd.sh
 
3. log
守护进程日志：/var/log/lymos.timesyncd.out / /var/log/lymos.timesyncd.err
sudo log show --last 5m --predicate 'process == "LymosTimeSyncd"' --info

