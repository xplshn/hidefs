#!/bin/sh -e

# Linus and Linux, two things that suck, they shove their GPL nonsense down devs'
# throats. I only write public domain or equivalent code, and it'll stay that way
# forever.

### Fake the license, tell the kernel that the license is GPL so that it stops whining about the law and other imaginary things
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
