#!/bin/bash
# SPDX-License-Identifier: CC0
# Copyright 2025 John Watts <contact@jookia.org>

set -e
set -x
TODAY="$(date +"%Y%m%d")"
FILE=uboot-jookia-$TODAY.tar.bz2
sed -i "s/\(PATCHESVERSION = \).*/\1-jookia$TODAY/g" Makefile
git commit -m "Bump PATCHESVERSION to -jookia$TODAY" -s -S Makefile
git tag -s -m "Release $TODAY" jookia/$TODAY
mkdir -p release/
git archive --format=tar --prefix=uboot-jookia-$TODAY/ HEAD | \
	bzip2 > release/$FILE
cp jookia/allowed_signers release
ssh-keygen -Y sign -f ~/.ssh/id_ed25519_sk_solo.pub -n file release/$FILE
(cd release; sha256sum * > sha256sums)
git push mine jookia_main jookia/$TODAY
cat <<EOF >release/GITHUB
(add some comment here)

How to check the integrity and authenticity of these files:

\`\`\`
sha256sum -c sha256sums
ssh-keygen -Y verify -f allowed_signers -I jookia -n file -s $FILE.sig < $FILE
\`\`\`

Make sure to check the key fingerprint against my website: https://www.jookia.org/wiki/Keys
EOF
