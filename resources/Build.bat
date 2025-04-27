bannertool.exe makebanner -i banner.png -a audio.wav -o banner.bnr
bannertool.exe makesmdh -s "CtrRGBPAT2" -l "LED customisation for 3DS" -p "CPunch & Golem64" -i icon.png  -o icon.icn
makerom -f cia -o CtrRGBPAT2.cia -DAPP_ENCRYPTED=false -rsf template.rsf -target t -exefslogo -elf CtrRGBPAT2.elf -icon icon.icn -banner banner.bnr