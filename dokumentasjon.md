
client_list.c er en fil som lar oss lage lenkelister med client-structs. Separert i egen fil ettersom både server og client kan ha nytte av funkjsonaliteten

#### include:
antar at det er lov til å bruke biblioteksfunksjoner på alt som ikke innebærer å sende UDP-pakken
## input-sjekker:
TODO: skriv om hvilke inputs vi sjekker og hvilke vi ikke sjekker. heh.
-
# server
### input-sjekker:
- Dersom server mottar en melding som ikke er på formen `PKT %8d %6s %20s`, så vil meldingen bli ignorert.


#### sockets:
sockfd: den eneste socketen vi bruker. Sender og mottar meldinger fra klienter
Ettersom oppgaven spesifiserer at serveren skal kjøre for alltid, er den eneste måten å avslutte den å bruke ctrl + c i terminalen. Selvom det er lagt inn en linje for å lukke sockets, vil vi aldri nå dette, men det antar jeg går greit.

# client

#### sockets:
sockfd: sender og mottar meldinger til server og klienter
STDIN_FILENO: leser meldinger fra stdin

#### sequence_number
Velger å ha stigende nummer. Antar at det ikke vil sendes så mange meldinger i én økt at vi får integer overflow.

Klienten har en global variabel hvor den lagrer sitt eget sequence number. Det er dette vi bruker når vi sender meldinger til andre, og for hver melding vi sender, øker vi den med én.
For hver kjente klient som er lagret i known_clients, lagrer vi det høyeste sequence number vi har mottatt for den gjeldende klienten. Når vi mottar en melding, velger vi bare å skrive den ut til stdout dersom sequence number er høyere enn det vi har fått tidligere fra samme klient.

#### Reg-melding:
skjer IKKE i en while-løkke fordi oppgaven sier at reg-melding bare skal skje én gang (uavhengig om det er vellykket elle rikke).

#### mottak av melding
Jeg velger å bare registrere en klient i cache dersom vi forsøker å sende en utgående melding til vedkommende. Altså legger vi de ikke til i cache dersom de sender en melding til oss.

#### QUIT:
I Cbra-videoene skjer klientens hendelssesløkke slik at den fortsetter så lenge stdin ikke er "QUIT". Ulempen med dette er at når bruker skriver inn QUIT, må vi legge til en ekstra sjekk for å forhindre at QUIT-meldingen går til server, som er unødvendig. Derfor setter jeg heller Hendelsesløkken i en while(1), og legger inn i hendelsesløkken at input "QUIT" bryter ut av hendelsesløkkenn.

#### Teksrespons
Oppgaven sier at dersom en klient mottar en melding fra en annen klient som har feil format, så skal vi svare med ACK nummer WRONG FORMAT. Men ettersom formatet er feil, er det ikke sikkert at vi har motatt et nummer i det hele tatt. I praksis vil dette neppe skje, så jeg velger å ikke håndtere det ettersom det skyldes en design-feil i oppgaven.

Oppgaven sier ikke hvordan Ack nummer WRONG FORMAT og ACK nummer WRONG NAME skal behandles hos klienten som mottar de. Antar derfor at det er greit at klienten ignorerer de, og sender heller melding på nytt i håp om å motta ACK nummer OK.

### Meldinger mellom Klienter
Dette fungerer, og det er implementert stop-and-wait slik som oppgaven beskriver. Det er ikke lagret en lenkeliste for utgående meldinger, men programmet leser fra stdin etter behov. Ettersom programmet bruker flere nøstede select'er, leses meldingene automatisk inn ved behov uten at det trengs å implementeres.

Dersom klient A sender en melding til klient B, og A venter på ACK fordi en pakke forsvinner, så kan klient C sende pakker til både A og B uten problem. Men dersom klient B vil sende en ny melding til A, før A har mottatt sin ACK, så vil meldingen som B sendte gå tapt. Men dette antar jeg er greit ettersom brukeren bak klient B får beskjed om at A er UNREACHABLE, og kan derfor velge å skrive inn meldingen på nytt dersom det er ønskelig.

Eneste som feiler er at dersom vi mottar en melding på socket imens vi venter på en ACK fra samme klient, så forsvinner en pakke, og vi får at NICK er UNREACHABLE på begge sider. For det er typisk det som skjer dersom en ACK forsvinner: mottager av melding har sendt acken sin og er klar for å sende en ny melding. Men Avsender av meldingen venter fortsatt på acken, og er ikke klar for en ny melding.
Mulig løsninger:
Klient som venter på ACK men mottar vanlig melding kan behandle det som en vanlig melding og sende ACK og sånn. Men da må det legges inn i await_ack_from_client og det tror jeg kan bli rotete? Eller den kan returnere en int som indikerer at det er det som har skjedd. Eller vi kan ha en egen metode send_ack_to_client.

Hvis vi mottar en ikke-ACK fra klienten vi forventer ACK fra, så betyr det at melding eller ACK har forsvunnet på veien. Og det er det samme som en timeout! Så vi bør behandle det som en timeout.


#### Når skal vi gjøre lookups?
I min besvarelse velger vi å bare gjøre lookup når vi skal sende MSG, ikke når vi skal sende ACK (da leser vi socket fra recvfrom). Det er fordi faglærer sa dette på mattermost, på tross av at Alice gjør LOOKUP på Bob før hun sender en ACK i eksempelscenariet.

# block_list
Lager en egen struct for blokkerte klienter fordi de er så mye enklere enn klientene i client_list ettersom de bare trenger å inneholde nick.


# input-sjekker
Burde de gjøres hos client istedenfor server? så slipper server å sjekke input. Nei, for dte kan oppstå feil underveis i sendingen, som vi må sjekke for.

# avvik

# spørsmål:
- Skal vi ha noen form for tapsgjenoppretting hos server? Den sender jo bare ACK's. Alt gjøres vel hos client?
- Hva gjør vi hvis sequence_number overstiger størrelsen på en int? Kan vi anta at det ikke skjer? Evt bare bruke 0 og