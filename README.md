# Daemon synchronizujący dwa katalogi
Korzystający z wywołań systemowych Linux. Działa w tle wybudzając się co pewną ilość czasu. Kopiuje wszystkie nowe lub zmodyfikowane pliki z katalogu źródłowego do docelowego. Usuwa pliki z katalogu docelowego które nie istnieją w katalogu źródłowym.


UŻYCIE: ./a.out katalog_zrodlowy katalog_docelowy [-R] [-t CZAS] [-m PRÓG_MB]

[-R] daemon ma działać rekurencyjnie (przeglądać podkatalogi)

[-t CZAS] czas co jaki ma się wybudzać

[-m PRÓG_MB] od ilu MB ma kopiować pliki z wykorzystaniem mapowania
