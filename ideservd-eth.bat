@echo off
REM   This script enables user to start ideservd for ethernet

DEL ideservd.log

REM
REM Set the IP address of C64, choose any unused address

SET C64IP=192.168.0.64

REM
REM Set the network address of C64 (hex, 00-0F)

SET NET=00

REM
REM Set the network interface name

SET INTF="Local Area Connection"

REM WIN7
netsh interface ipv4 add neighbors %INTF% %C64IP% 49-44-45-36-34-%NET%

REM XP
arp -s %C64IP% 49-44-45-36-34-%NET%
ideservd.exe -i %C64IP% -N 0x%NET% -m eth
