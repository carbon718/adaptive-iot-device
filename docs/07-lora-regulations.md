# Regulacje prawne dotyczące LoRa

## Pasmo ISM

LoRa to technologia bezprzewodowej komunikacji dalekiego zasięgu o niskim poborze energii. Działa w pasmach ISM (Industrial, Scientific and Medical) - w Europie najczęściej 868 MHz, w Ameryce Północnej 915 MHz, a w niektórych regionach 433 MHz.

W Europie pasmo 868 MHz jest ogólnodostępne i nie wymaga pozwolenia radiowego, pod warunkiem przestrzegania limitów mocy nadawania i czasu zajętości kanału (duty cycle). Szczegółowe wymagania określa m.in. Decyzja Komisji z dnia 9 listopada 2006 r. w sprawie harmonizacji widma radiowego na potrzeby urządzeń bliskiego zasięgu (notyfikowana jako dokument nr C(2006) 5304).

## Duty cycle

Decyzja definiuje aktywność nadajnika następująco:

> „Aktywność nadajnika” oznacza wyrażony w procentach stosunek Σ(Ton)/(Tobs), gdzie Ton oznacza czas włączenia jednego nadajnika, a Tobs - okres obserwacji. Ton jest mierzony w obserwowanym zakresie częstotliwości (Fobs). Jeżeli w niniejszym załączniku technicznym nie określono inaczej, Tobs jest ciągłym jednogodzinnym przedziałem czasu, a Fobs - odnośnym zakresem częstotliwości uwzględnionym w niniejszym załączniku technicznym. Możliwość określenia mniej restrykcyjnych warunków w rozumieniu art. 3 ust. 3 oznacza, że państwa członkowskie mogą dopuścić wyższą wartość „aktywności nadajnika”.

Dla wykorzystywanego podpasma (868,0 MHz, moc 14 dBm) oznacza to, że w dowolnym ciągłym okresie jednej godziny urządzenie może nadawać łącznie przez maksymalnie 1% czasu, czyli 36 s.

Ma to bezpośredni wpływ na implementację algorytmu - po każdej transmisji LoRa kolejna wysyłka tym interfejsem musi być zablokowana do czasu, aż nie spowoduje przekroczenia limitu duty cycle.

## Źródła

- [Rozporządzenie Ministra Cyfryzacji z dnia 12 listopada 2026 r. w sprawie rozszerzonego zakresu urządzeń radiowych nadawczych lub nadawczo-odbiorczych, które mogą być używane bez pozwolenia radiowego (Dz. U. poz. 1500)](https://sip.lex.pl/#/act/22325107/448353276/rozszerzony-zakres-urzadzen-radiowych-nadawczych-lub-nadawczo-odbiorczych-ktore-moga-byc-uzywane...?keyword=Rozporz%C4%85dzenie%20Ministra%20Cyfryzacji%20w%20sprawie%20urz%C4%85dze%C5%84%20radiowych%20nadawczych%20lub%20nadawczo-odbiorczych,%20kt%C3%B3re%20mog%C4%85%20by%C4%87%20u%C5%BCywane%20bez%20pozwolenia%20radiowego&searchPit=2026-11-20)
- [Decyzja Komisji z dnia 9 listopada 2006 r. w sprawie harmonizacji widma radiowego na potrzeby urządzeń bliskiego zasięgu (Dz. U. UE L z 2006 r. Nr 312, str. 66 z późn. zm.)](https://sip.lex.pl/#/act/67643737/176490036/decyzja-2006-771-we-w-sprawie-harmonizacji-widma-radiowego-na-potrzeby-urzadzen-bliskiego-zasiegu?keyword=Decyzja%20Komisji%202006%2F771%2FWE&searchPit=2026-11-20)
- ETSI EN 300 220-1 oraz ETSI EN 300 220-2
- ERC Recommendation 70-03
