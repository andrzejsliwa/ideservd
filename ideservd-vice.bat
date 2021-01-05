@echo off
REM   This script enables user to start ideservd for VICE

DEL ideservd.log

ideservd.exe -m vice
