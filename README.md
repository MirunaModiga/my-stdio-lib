Structura de tip SO_FILE descrie un fisier deschis, fiind folosita de toate celelalte functii si are urmatoarele campuri:

* `_fd`           - file descriptor pentru fisierul deschis
* `_mode`         - variabila sa retin modul in care deschid fisierul
* `_offsetFile`   - cursor in fisierul deschis
* `_offsetBuffer` - cursor in buffer
* `_bytesRead`    - numar de bytes ocupati in buffer
* `_writeFlag`    - flag setat daca s-a efectuat o operatie de scriere (pentru citire e setat cu 0)
* `_error`        - flag pentru eroare (setat cu -1 cand am eroare)
* `_eof`          - flag setat cand verific daca am ajuns la finalul fisierului
* `_childPid`     - pid-ul procesului copil creat prin functia popen
* `_buffer`       - buffer asociat structurii, folosit pentru operatiile de citire/scriere

Functiile din fisierul so_stdio.h sunt implementate in fisierul so_stdio.c si descriu urmatoarele functionalitati:

* `so_fopen`    - aloc structura fisierului, verific modul de deschidere dat ca parametru, si initializez membrii asociati 
                structurii SO_FILE si deschid fisierul
* `so_fclose`   - fac fflush daca ultima operatie a fost de scriere (_writeFlag setat), inchid fisierul si eliberez memoria
* `so_fileno`   - intorc file descriptor-ul asociat fisierului
* `so_fflush`   - verific daca am deschis fisierul in modul "a" sau "a+", mut cursorul la finalul fisierului
                - verific daca ultima operatie a fost de scriere si scriu continutul buffer-ului in fisier si apoi golesc buffer-ul
* `so_fseek`    - muta cursorul fisierului la o anumiota pozitie, cu ajutorul apelului lseek
                - daca este cazul, scriu continutul buffer-ului in fisier si apoi golesc buffer-ul
* `so_ftell`    - intoarce pozitia curenta din fisier (_offsetFile)
* `so_fread`    - citesc pentru fiecare caracter, pe fiecare octet cu ajutorul functiei fgetc din buffer-ul deja populat
                - tratez cazurile de eroare (ale apelurilor de sistem din fgetc sau daca am ajuns la final)
                - pun in ptr caracterele citite si umar cate caractere citesc
* `so_fwrite`   - cu ajutorul functiei fputc, pun fiecare caracter din buffer-ul pe care vreau sa il scriu in fisier, in buffer-ul 
                asociat structurii mele
                - dupa ce umplu buffer-ul meu, il scriu pe tot odata in fisier (fflush) -> buffering
                - tratez cazurile de eroare si numar cate caractere am pus in buffer
* `so_fgetc`    - verific daca am permisiune de read, daca nu => eroare
                - verific daca s-a efectuat o operatie de scriere, scriu continutul buffer-ului in fisier ca sa golesc buffer-ul (fflush)
                - setez flag-ul _writeFlag pe 0, pentru ca se face citire
                - pot sa citesc in buffer doar daca acesta este gol => apelez read => pun caracterul citit in buffer la offset-ul 
                curent din buffer, pe care il incrementez dupa citire
                - daca read intoarce 0, am ajuns la finalul fisierului => setez flagul _eof
* `so_fputc`    - verific sa am permisiune de scriere, daca nu => eroare
                - daca am umplut tot buffer-ul, il scriu in fisier (fflush) inainte sa mai pun vreun caracter in el
                - setez flag-ul _writeFlag pe 1, pentru ca se face scriere
                - pun caracterul in buffer la offset-ul curent si incrementez cursorul
* `so_feof`     - returnez flag-ul _eof, pe care l-am setat cand am verificat in fgetc daca apelul read intoarce 0
* `so_ferror`   - returnez flag-ul _error, pe care l-am setat in fiecare functie in cazul tratarii unei erori
* `so_popen`    - in procesul parinte, imi initializez structura asociata fisierului
                - creez pipe-ul si procesul copil, tratand si cazurile de eroare
                - in procesul copil, verific parametrul type, fac redirectarile dupa caz, _fd primeste valoarea capatului pipe-ului dupa "type"
                        => daca sunt in modul "r" : _fd primeste capatul de citire
                                                    redirectez stdout in capatul de scriere => ce scriu in pipe citesc in fisier
                                                    inchid capatul de scriere
                        => daca sunt in modul "w" : _fd primeste capatul de scriere
                                                    redirectez stdin in capatul de citire => ce citesc in pipe scriu in fisier
                                                    inchid capatul de citire
                - execut comanda in procesul copil si returnez structura in procesul parinte
                - in ambele procese, inchid capetele nefolosite ale pipe-ului
* `so_pclose`   - astept terminarea procesului copil si eliberez memoria ocupata de structura SO_FILE

