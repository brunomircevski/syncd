# Directory syncing linux daemon
The software is using system calls. It works in the background, waking up every some time. It copies all new and modified files from source directory do destination directory. It also removes files that are no longer in source directory.

USAGE: ./a.out source destination [-R] [-t TIME] [-m THRESHOLD_MB]

[-R] recursive mode (look into subdirectories)

[-t TIME] sleep time

[-m THRESHOLD_MB] from how many MB should copy files using mapping

# Daemon linux synchronizujący dwa katalogi
Program korzysta z wywołań systemowych Linux. Działa w tle wybudzając się co pewną ilość czasu. Kopiuje wszystkie nowe lub zmodyfikowane pliki z katalogu źródłowego do docelowego. Usuwa pliki z katalogu docelowego które nie istnieją w katalogu źródłowym.

UŻYCIE: ./a.out katalog_zrodlowy katalog_docelowy [-R] [-t CZAS] [-m PRÓG_MB]

[-R] daemon ma działać rekurencyjnie (przeglądać podkatalogi)

[-t CZAS] czas spania

[-m PRÓG_MB] od ilu MB ma kopiować pliki z wykorzystaniem mapowania
