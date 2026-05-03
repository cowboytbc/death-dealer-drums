@echo off
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe" -g -G -o -c "!analyze -v;k 20;q" "C:\Users\crypt\OneDrive\Desktop\DEATH DEALER DRUMS\build\windows\DeathDealerDrums_artefacts\Debug\Standalone\Death Dealer Drums.exe" > "%~dp0crash_log.txt" 2>&1
echo Done. See crash_log.txt
