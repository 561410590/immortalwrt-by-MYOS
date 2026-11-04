#!/bin/sh
# Offline fixture only. Never access a modem or start a service.
printf 'cpetools\t%s\n' "$*" >> "$QA_TRACE"
