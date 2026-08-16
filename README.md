# KPLive Ads Keyer

OBS-Studio-Plugin für eine dauerhaft zuschaltbare Werbeszene über dem normalen
Programmbild. Die mitgelieferte Quelle **Programmbild / Vorschau** kann
in der Werbeszene frei skaliert und hinter einer Overlay-Grafik platziert werden.

Das Plugin basiert auf dem GPL-2.0-Projekt
[`obs-downstream-keyer`](https://github.com/exeldro/obs-downstream-keyer) 0.4.4
von Exeldro und enthält eine abgesicherte Quellen-Lebensdauer sowie eine direkte
Auswahl zwischen Programm und Vorschau.

## Einrichtung für `ads_prime_overlay.png`

1. In OBS eine neue Szene, zum Beispiel **Werbung – Twitch Prime**, erstellen.
2. Darin die Quelle **Programmbild / Vorschau** hinzufügen und
   **Aktuelles Programmbild** wählen.
3. Die Quelle auf Position **X 0 / Y 0** und Größe **1705 × 959 px** setzen.
4. `ads_prime_overlay.png` als Bildquelle darüber legen, Position **0 / 0**,
   Größe **1920 × 1080 px**. Die transparente Fläche der PNG ist exakt
   1705 × 959 px groß.
5. Im Dock **KPLive Werbe-Keyer** über `+` diese Werbeszene hinzufügen. Ein Klick
   auf die Szene blendet sie global ein; **Aus** nimmt sie wieder heraus. Die
   normalen Programmszenen bleiben währenddessen weiterhin aktiv und können
   normal gewechselt werden.

Die Quelle unterstützt alternativ das aktuelle Vorschaubild. Ein- und
Ausblenden lässt sich zusätzlich unter **Einstellungen →
Hotkeys** belegen.

## Quellenanimationen (ab 0.3.0)

Für jede im Werbe-Keyer eingetragene Werbeszene kann eine Quelle separat animiert
werden. Dazu die Werbeszene im Dock auswählen und über das Zahnrad
**Quellenanimationen...** öffnen. Verfügbar sind Ein- und Ausblend-Presets von
links, rechts, oben und unten sowie Zoom- und Zoom/Dreh-Animationen. Dauer und
Easing können getrennt eingestellt werden.

Die aktuell in OBS eingestellte Transformation der Quelle bleibt immer die
Zielposition. Beim Einblenden startet die Quelle am gewählten Preset-Punkt und
fährt auf ihre OBS-Position. Beim Ausblenden läuft sie vom OBS-Zustand zum
gewählten Endpunkt; danach stellt das Plugin die ursprüngliche Transformation
automatisch wieder her. Auch Quellen innerhalb von OBS-Gruppen können ausgewählt
werden. Die Animationseinstellungen werden mit der Szenensammlung gespeichert.

Ist eine Quellen-Ausblendanimation aktiv, übernimmt sie das Ausblenden des
Werbe-Keyers; ein zusätzlich konfigurierter DSK-Ausblendübergang wird für diesen
Vorgang nicht noch einmal darübergelegt.

## Dauerhafter Soll-Zustand

Die zuletzt bewusst gewählte Einstellung ist verbindlich. Ist eine Werbeszene
aktiviert, überwacht das Plugin den tatsächlichen OBS-Ausgabekanal und stellt
die Werbeszene automatisch wieder her, falls OBS sie verliert. Wird **Aus**
gewählt, bleibt der Keyer ausgeschaltet. Dieser Zustand wird mit der
Szenensammlung gespeichert. Bewusst ausgeschlossene Szenen bleiben davon
unberührt.

## Installation

- `ads-keyer.dll` nach `obs-plugins/64bit/`
- den Ordner `ads-keyer` nach `data/obs-plugins/`

OBS muss beim Kopieren geschlossen sein.
