#!/bin/sh -e

SEED="_$RANDOM_$(date +%s)"
sed_in_place() {
  cp "$1" "$1.$SEED.bak"
  sed "$2" "$1.$SEED.bak" > "$1"
}
FILE="./hidefs.c"
LICENSE_LINE="MODULE_LICENSE\(\"GPL\"\);"

trap 'if [ -f "$FILE.$SEED.bak" ]; then mv "$FILE.$SEED.bak" "$FILE"; fi' EXIT

sed_in_place "$FILE" "s/^MODULE_LICENSE.*/$LICENSE_LINE/"


# ACTUAL BUILD CMDS ###
make $@
#######################

mv "$FILE.$SEED.bak" "$FILE"

trap - EXIT
