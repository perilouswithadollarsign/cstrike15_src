#!/bin/bash

# rm -rf /tmp/symbols/
./breakpad_build_symbols.py ~/Perforce/perforce_1666/yahn_mac/Steam/main/src/ /tmp/symbols
./breakpad_build_symbols.py ~/Perforce/perforce_1666/yahn_mac/Steam/main/client/osx32/ /tmp/symbols
#./breakpad_build_symbols.py ~/Perforce/perforce_1666/yahn_mac/Steam/main/client/ /tmp/symbols
#./breakpad_build_symbols.py ~/Perforce/perforce_1666/yahn_staging/game/ /tmp/symbols
./breakpad_build_symbols.py ~/Perforce/perforce_1666/yahn_staging/src/ /tmp/symbols
rsync -r /tmp/symbols/ socorro-test:./symbols

