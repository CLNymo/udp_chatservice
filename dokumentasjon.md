
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

#### Reg-melding:
skjer IKKE i en while-løkke fordi oppgaven sier at reg-melding bare skal skje én gang (uavhengig om det er vellykket elle rikke).

#### mottak av melding
Jeg velger å bare registrere en klient i cache dersom vi forsøker å sende en utgående melding til vedkommende. Altså legger vi de ikke til i cache dersom de sender en melding til oss. 

#### QUIT:
I Cbra-videoene skjer klientens hendelssesløkke slik at den fortsetter så lenge stdin ikke er "QUIT". Ulempen med dette er at når bruker skriver inn QUIT, må vi legge til en ekstra sjekk for å forhindre at QUIT-meldingen går til server, som er unødvendig. Derfor setter jeg heller Hendelsesløkken i en while(1), og legger inn i hendelsesløkken at input "QUIT" bryter ut av hendelsesløkkenn.

#### Teksrespons
Oppgaven sier at dersom en klient mottar en melding fra en annen klient som har feil format, så skal vi svare med ACK nummer WRONG FORMAT. Men ettersom formatet er feil, er det ikke sikkert at vi har motatt et nummer i det hele tatt. Programmet vil derfor avsluttes dersom dette skulle skje, men det antar jeg er greit ettersom det skyldes en design-feil i oppgaven.

Oppgaven sier ikke hvordan Ack nummer WRONG FORMAT og ACK nummer WRONG NAME skal behandles hos klienten som sendte feil melding i utgangspunktet. Velger derfor at dersom man mottar ACK nummer WRONG NAME eller ACK nummer WRONG FORMAT, så skriver vi dette ut til brukeren, som kan velge om de vil prøve å sende på nytt eller ikke.


# input-sjekker
Burde de gjøres hos client istedenfor server? så slipper server å sjekke input. Nei, for dte kan oppstå feil underveis i sendingen, som vi må sjekke for.

# avvik

# spørsmål:
- Skal vi ha noen form for tapsgjenoppretting hos server? Den sender jo bare ACK's. Alt gjøres vel hos client?
- Hva gjør vi hvis sequence_number overstiger størrelsen på en int? Kan vi anta at det ikke skjer? Evt bare bruke 0 og