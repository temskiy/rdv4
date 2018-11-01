#!/usr/bin/env bash
cat ../client/default_keys.dic ./*.keys |sed -r '/^#/d'|sed '/^$/d'|awk -F ",|#" '{print $1}'|tr '[:lower:]' '[:upper:]'|sort|uniq > my_keys.dic

