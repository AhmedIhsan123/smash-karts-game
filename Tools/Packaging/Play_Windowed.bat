@echo off
rem Starts the game in a 1280x720 window (handy for running two copies on one PC).
start "" "%~dp0SmashKarts.exe" -windowed -ResX=1280 -ResY=720 %*
