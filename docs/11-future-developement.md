# Dalsze możliwości rozwoju

## Adaptacja parametrów LoRa

Algorytm można rozbudować tak, aby adaptacyjnie dobierał parametry transmisji LoRa: współczynnik rozpraszania (SF), szerokość pasma (BW), współczynnik kodowania (CR) oraz moc nadawania. Na przykład przy dobrych warunkach (wysokie RSSI i SNR z ostatniego ACK) można obniżyć SF i moc nadawania. Skraca to czas nadawania, a co za tym idzie zmniejsza zużycie energii i czas blokady duty cycle.

## Szacowanie czasu nadawania i zużycia energii

Bardziej zaawansowana wersja algorytmu mogłaby szacować czas nadawania (ToA) na podstawie rozmiaru payloadu, a następnie przewidywać zużycie energii dla każdego z łączy. Pozwoliłoby to dokładniej optymalizować zużycie energii przy wyborze łącza.

Warto jednak zastanowić się, na ile tak dokładne szacowanie jest przydatne w typowych zastosowaniach IoT. Większość urządzeń wysyła dane stosunkowo rzadko, nie wymaga krótkiego czasu reakcji, a niemal zawsze preferowane jest łącze bardziej energooszczędne. Rozwiązanie to mogłoby się natomiast sprawdzić w urządzeniach przesyłających dane o zróżnicowanej wielkości, np. w czujniku, który zazwyczaj wysyła krótkie pomiary, ale od czasu do czasu musi przesłać większą porcję danych, np. nagranie głosowe.
