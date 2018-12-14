#/bin/bash

UUID=$(uuidgen | sed s/-/_/g | tr a-z A-Z)
echo "HEADER_${UUID}"
