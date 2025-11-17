#! /bin/bash

# Script fatto per assicurarsi che ogni cosa sia fatta a modo
# prima di un commit nella repository

# Flow:
# Hai controllato di aver fatto un pull?
# Il codice da' degli errori di compilazione?
# Il codice passa i test?
# Il codice e' commentato?

bold='\e[1m'
offbold='\e[0m'

red='\e[31m'
green='\e[32m'
yellow='\e[1;33m'
offcolor='\e[0m'

function ask_question() {
  printf "%b" "$1"
  read -n 1 -s key

  if [[ $key != 'y' ]]; then
    printf "Eh non va bene!\n"
    exit 1
  else
    printf "✅ Ottimo, passiamo alla prossima domanda!\n\n"
  fi
}

ask_question "Hai controllato di aver fatto ${bold}git pull${offbold}? y/N\n"
ask_question "Il codice compila ${red}senza errori${offcolor} di compilazioni? y/N\n"
ask_question "Il codice passa i ${yellow}test${offcolor}? y/N\n"
ask_question "Hai ${green}commentato${offcolor} il nuovo codice? y/N\n"

printf "🧙🏼 Il concilio ha parlato, puoi fare il commit!"
