
client_list.c er en fil som lar oss lage lenkelister med client-structs. Separert i egen fil ettersom både server og client kan ha nytte av funkjsonaliteten

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


#### QUIT:
I Cbra-videoene skjer klientens hendelssesløkke slik at den fortsetter så lenge stdin ikke er "QUIT". Ulempen med dette er at når bruker skriver inn QUIT, må vi legge til en ekstra sjekk for å forhindre at QUIT-meldingen går til server, som er unødvendig. Derfor setter jeg heller Hendelsesløkken i en while(1), og legger inn i hendelsesløkken at input "QUIT" bryter ut av hendelsesløkkenn.

# input-sjekker
Burde de gjøres hos client istedenfor server? så slipper server å sjekke input. Nei, for dte kan oppstå feil underveis i sendingen, som vi må sjekke for.

# spørsmål:
- Skal vi ha noen form for tapsgjenoppretting hos server? Den sender jo bare ACK's. Alt gjøres vel hos client?
- Hva gjør vi hvis sequence_number overstiger størrelsen på en int? Kan vi anta at det ikke skjer? Evt bare bruke 0 og 1