#!/bin/bash
sudo ipcs -m | grep `whoami` | awk '{ print $2 }' | xargs -n1 ipcrm -m
sudo ipcs -s | grep `whoami` | awk '{ print $2 }' | xargs -n1 ipcrm -s
sudo ipcs -q | grep `whoami` | awk '{ print $2 }' | xargs -n1 ipcrm -q