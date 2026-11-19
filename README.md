# UKSW Hero

## Projekt

UKSW Hero to projekt proof of concept gry typu arena survival osadzonej na kampusie Uniwersytetu Kardynała Stefana Wyszyńskiego przy ul. Wóycickiego. Projekt składa się z natywnego klienta w C++ oraz serwera REST API w Go, który zapisuje sesje i event-y w PostgreSQL.
Projekt powstał w ramach przedmiotu Projekt Programistyczny Grupowy. 

## Gra

Gracz steruje studentem walczącym z goblinami na kampusie UKSW. Celem jest przetrwanie kolejnych fal przeciwników.

Dostępne mechaniki:

- Poruszanie się po mapie i kolizje z przeszkodami.
- Kamera podążająca za graczem.
- Atak kierunkowy i system zdrowia.
- Animacje postaci, walki i śmierci.
- Fale goblinów pojawiające się co 10–15 sekund.
- Wyszukiwanie drogi przeciwników algorytmem A*.
- Wyświetlanie zdrowia, liczby zabitych goblinów i identyfikatora sesji.

| Klawisz | Działanie |
| --- | --- |
| W, A, S, D | Poruszanie |
| Spacja | Atak |
| Enter | Rozpoczęcie gry, ręczne przejście do ekranu końcowego lub powrót do menu |
| Esc | Zamknięcie gry |
| Tab, przytrzymanie | Podgląd przeszkód i kolizji |
| G, przytrzymanie | Podgląd siatki nawigacyjnej |
| P | Testowe uśmiercenie gracza |

Po wpisaniu nazwy i rozpoczęciu gry klient próbuje utworzyć sesję na serwerze. Brak połączenia z API nie blokuje lokalnej rozgrywki.

Gra ma 3 widoki:

1. Start
![Start](client/assets/images/1_start.png)

2. Gameplay
![Start](client/assets/images/2_gameplay.png)

3. Game over
![Start](client/assets/images/3_game_over.png)

## Architektura

```text
uksw-hero/
├── client/
│   ├── assets/              # Mapa, sprite’y i czcionka
│   ├── src/
│   │   ├── characters/      # Student i gobliny
│   │   ├── systems/         # Nawigacja, wyszukiwanie drogi, fale goblinów i zdarzenia
│   │   ├── network/         # Komunikacja HTTP
│   │   └── main.cpp         # Pętla gry
│   └── CMakeLists.txt
├── server/
│   ├── api/                 # Obsługa HTTP i walidacja
│   ├── session/             # Modele, reguły i interfejs store'a
│   ├── storage/             # PostgreSQL i MemoryStore
│   ├── main.go
│   ├── go.mod
│   └── Dockerfile
├── db/
│   └── init.sql             # Początkowy schemat bazy
└── docker-compose.yml       # Kontenery API i PostgreSQL
```

### Klient

Klient korzysta z C++17 i raylib. `main.cpp` zarządza ekranami, zasobami, kamerą oraz aktualizacją i rysowaniem obiektów.

Postacie emitują zdarzenia przez funkcję zwrotną `GameplayEventHandler`. Przekazanie ich do `ApiClient` odbywa się w `main.cpp`, dzięki czemu klasy postaci nie zależą od HTTP ani identyfikatorów sesji.

Uproszczony diagram klas:

```mermaid
classDiagram
    class Student {
        -int health
        -Vector2 position
        +Update()
        +Draw()
        +TakeDamage()
        +GetHealth()
        +IsDead()
    }
    class Goblin {
        -int health
        -Vector2 position
        -vector~Vector2~ path
        +Update()
        +Draw()
        +TakeDamage()
        +IsAlive()
    }
    class GoblinSpawner {
        -int goblinsPerWave
        -float spawnInterval
        +Update()
        +SetGoblinsPerWave()
    }
    class NavigationGrid {
        +IsWalkable()
        +WorldToGrid()
        +GridToWorld()
        +UpdateObstacles()
    }
    class Pathfinding {
        +FindPath()$
    }
    class GameplayEventHandler {
        <<callback>>
    }
    class ApiClient {
        -string baseUrl
        -thread worker
        +QueueCreateSession()
        +QueueEvent()
        -ProcessRequests()
    }
    Student ..> Goblin : atakuje
    Goblin ..> Student : sciga i atakuje
    Goblin ..> Pathfinding : wyznacza trase
    Goblin ..> NavigationGrid : korzysta
    Pathfinding ..> NavigationGrid : sprawdza przejscia
    GoblinSpawner ..> Goblin : tworzy
    Student ..> GameplayEventHandler : emituje zdarzenia
    Goblin ..> GameplayEventHandler : emituje zdarzenia
    GoblinSpawner ..> GameplayEventHandler : przekazuje do goblinow
    GameplayEventHandler ..> ApiClient : polaczenie w main.cpp
```

`ApiClient` przetwarza żądania kolejno w osobnym wątku, aby komunikacja sieciowa nie zatrzymywała renderowania. Kolejka mieści osiem oczekujących żądań. Limit połączenia wynosi sekundę, a całego żądania trzy sekundy.

Zdarzenia są wysyłane bez automatycznych ponowień. Mogą zostać pominięte przy błędzie utworzenia sesji, przepełnieniu kolejki lub zamknięciu klienta. Klient nie wykorzystuje odpowiedzi na zdarzenia do synchronizacji lokalnego stanu gry.

### Serwer

Serwer jest aplikacją Go korzystającą z Gin. Kod dzieli się na trzy warstwy:

- `api` — routing, odczyt i walidacja JSON oraz odpowiedzi HTTP.
- `session` — modele sesji i zdarzeń, reguły biznesowe oraz interfejs `SessionStore`.
- `storage` — implementacje przechowywania danych.

Aplikacja uruchamiana przez `main.go` korzysta z `PostgresStore`. `MemoryStore` umożliwia testowanie bez bazy danych.

Poniższy diagram przedstawia struktury i interfejs Go w notacji klas:

```mermaid
classDiagram
    class api {
        -SessionService service
        +createSession()
        +recordEvent()
        +endSession()
    }
    class SessionService {
        -SessionStore store
        +Create()
        +RecordEvent()
        +End()
    }
    class SessionStore {
        <<interface>>
        +Create()
        +Update()
        +UpdateWithEvent()
    }
    class PostgresStore {
        -pgxpool.Pool pool
        +Create()
        +Update()
        +UpdateWithEvent()
    }
    class MemoryStore {
        -Mutex mu
        -map sessions
        -Event[] events
        +Create()
        +Update()
        +UpdateWithEvent()
    }
    class Session {
        +string ID
        +string PlayerName
        +string DeviceInfo
        +Time StartedAt
        +Time EndedAt
        +int Health
        +int GoblinsKilled
    }
    class Event {
        +string SessionID
        +string Type
        +RawMessage Data
    }
    api --> SessionService
    SessionService --> SessionStore
    SessionService ..> Session
    SessionService ..> Event
    SessionStore <|.. PostgresStore
    SessionStore <|.. MemoryStore
    MemoryStore o-- Session
    MemoryStore o-- Event
    Event --> Session : SessionID
```

PostgreSQL przechowuje dane w tabelach `session` i `event`. Aktualizacja sesji i dodanie zdarzenia odbywają się w jednej transakcji. Blokada wiersza `FOR UPDATE` zabezpiecza równoczesne aktualizacje tej samej sesji. Magazyn pamięciowy korzysta z mutexu.

### Komunikacja klient–serwer

Klient wysyła żądania HTTP z danymi JSON. Domyślny adres API to `http://127.0.0.1:8080`.

```mermaid
sequenceDiagram
    actor Gracz
    participant Gra as Klient C++
    participant APIClient as ApiClient — osobny wątek
    participant API as Serwer Gin
    participant DB as PostgreSQL

    Gracz->>Gra: Wpisanie nazwy i rozpoczęcie gry
    Gra->>APIClient: QueueCreateSession(nazwa)
    Note over Gra: Rozgrywka trwa podczas komunikacji HTTP
    APIClient->>API: POST /session
    API->>DB: Zapis nowej sesji
    DB-->>API: Potwierdzenie
    API-->>APIClient: 201 JSON z session_id
    APIClient-->>Gra: Identyfikator przez future

    loop Zdarzenia rozgrywki
        Gra->>APIClient: QueueEvent(typ)
        APIClient->>API: POST /event
        API->>DB: Transakcja: aktualizacja sesji i zapis zdarzenia
        DB-->>API: Potwierdzenie
        API-->>APIClient: 200 JSON ze stanem sesji
        Note over APIClient: Odpowiedź zdarzenia jest pomijana
    end
```

| Endpoint | Działanie |
| --- | --- |
| `POST /session` | Utworzenie sesji z nazwą gracza i opcjonalnym `device_info`; odpowiedź `201` |
| `POST /event` | Zapis zdarzenia wskazanej sesji; odpowiedź `200` |
| `POST /session/:id/end` | Ustawienie czasu zakończenia sesji; odpowiedź `200` |

Obsługiwane zdarzenia:

| `event_type` | Efekt na serwerze |
| --- | --- |
| `player_attacked` | Zmniejszenie zdrowia o 10, do minimum 0 |
| `goblin_killed` | Zwiększenie liczby zabitych goblinów o 1 |
| `player_died` | Ustawienie czasu zakończenia sesji |

Klient wysyła `player_died` po śmierci postaci, ale obecnie nie wywołuje osobnego endpointu zakończenia sesji.

Błędy mają wspólny format:

```json
{
  "error": {
    "code": "session_not_found",
    "message": "session not found"
  }
}
```

## Wdrożenie i uruchomienie

Klient jest kompilowany i uruchamiany lokalnie. Serwer oraz PostgreSQL działają w oddzielnych kontenerach zarządzanych przez Docker Compose.

### Wymagania lokalne

- Kompilator C/C++ obsługujący C++17.
- CMake co najmniej 3.24 i narzędzie budowania, np. Make.
- Biblioteki deweloperskie libcurl i nlohmann/json.
- Biblioteki systemowe wymagane przez raylib.
- Środowisko graficzne z obsługą OpenGL.
- Docker Engine lub Docker Desktop oraz Docker Compose.
- Dostęp do internetu przy pobieraniu zależności i obrazów.
- Wolne porty lokalne `8080` i `5432`.

Go 1.27 jest potrzebne do lokalnego budowania i testowania serwera. Budowa serwera przez Docker korzysta z toolchainu wewnątrz obrazu.

### Uruchomienie serwera i bazy

W katalogu głównym projektu należy skonfigurować plik `.env`:

```dotenv
POSTGRES_USER=uksw
POSTGRES_PASSWORD=lokalne_haslo
POSTGRES_DB=uksw_hero
POSTGRES_URL=postgres://uksw:lokalne_haslo@postgres:5432/uksw_hero?sslmode=disable
```

Dane w `POSTGRES_URL` muszą odpowiadać pozostałym zmiennym. Nazwa hosta `postgres` wskazuje usługę wewnątrz sieci Docker Compose. Compose przekazuje ten adres serwerowi jako `DATABASE_URL`.

Uruchomienie z katalogu głównego:

```sh
docker compose up --build -d
docker compose ps
docker compose logs server
```

API jest dostępne pod adresem `http://127.0.0.1:8080`, a PostgreSQL lokalnie pod `127.0.0.1:5432`.

Skrypt `db/init.sql` tworzy tabele podczas pierwszej inicjalizacji pustego katalogu danych PostgreSQL. Dane są przechowywane w nazwanym wolumenie `postgres_data`; zmiana skryptu nie aktualizuje automatycznie istniejącej bazy.

Compose ustala kolejność startu usług, ale nie sprawdza gotowości bazy. Serwer sprawdza połączenie podczas startu i ma skonfigurowane automatyczne ponowne uruchamianie.

Zatrzymanie usług z zachowaniem wolumenu danych:

```sh
docker compose down
```

### Kompilacja i uruchomienie klienta

Przykładowe zależności systemowe dla Ubuntu:

```sh
sudo apt install build-essential cmake \
  libasound2-dev libx11-dev libxrandr-dev libxi-dev \
  libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev \
  libcurl4-openssl-dev nlohmann-json3-dev
```

Z katalogu głównego projektu:

```sh
cmake -S client -B client/build -DCMAKE_BUILD_TYPE=Release
cmake --build client/build --parallel
./client/build/uksw_hero
```

CMake korzysta z zainstalowanej biblioteki raylib albo pobiera raylib 5.5. Podczas budowania kopiuje katalog `client/assets` obok pliku wykonywalnego. Zasoby są ładowane względem położenia programu.

Zmiana adresu API:

```sh
UKSW_HERO_API_URL=http://127.0.0.1:8081 ./client/build/uksw_hero
```

Komunikacja sieciowa wymaga libcurl z obsługą asynchronicznego DNS. Bez niej klient uruchamia rozgrywkę bez połączenia z API.

## Technologie

| Technologia | Opis |
| --- | --- |
| C++17 | Język implementacji natywnego klienta i mechanik rozgrywki. |
| raylib | Biblioteka obsługująca okno, grafikę, wejście i podstawowe operacje gry. |
| CMake | Narzędzie konfigurujące kompilację klienta i kopiowanie zasobów. |
| Go | Język implementacji serwera i logiki obsługi sesji. |
| Gin | Framework Go zapewniający routing HTTP i obsługę żądań JSON. |
| pgx | Sterownik PostgreSQL dla Go, używany do zapytań, transakcji i puli połączeń. |
| PostgreSQL | Relacyjna baza przechowująca sesje i zdarzenia rozgrywki. |
| Docker i Docker Compose | Narzędzia budowania kontenera serwera oraz wspólnego uruchamiania API i bazy. |
| libcurl | Biblioteka realizująca żądania HTTP klienta. |
| nlohmann/json | Biblioteka serializacji i odczytu JSON w C++. |

## Testy

Testy serwera znajdują się obok implementacji w pakietach `api` i `storage`. Obejmują scenariusze obsługi sesji, walidację żądań, współbieżność, atomowość zmian i trwałość zapisu PostgreSQL.

Polecenia należy wykonywać z katalogu `server`:

```sh
go test ./...
go test -race ./...
go build ./...
```

Testy integracyjne PostgreSQL wymagają zmiennej `TEST_DATABASE_URL` wskazującej bazę testową:

```sh
TEST_DATABASE_URL='postgres://uksw:lokalne_haslo@127.0.0.1:5432/uksw_hero_test?sslmode=disable' \
  go test ./...
```

Baza testowa musi wcześniej istnieć, a użytkownik musi mieć uprawnienia do tworzenia schematów. Każdy test integracyjny tworzy osobny schemat na podstawie `db/init.sql` i usuwa go po zakończeniu. Bez `TEST_DATABASE_URL` testy integracyjne są pomijane.


## Zależności

| Obszar | Zależności bezpośrednie |
| --- | --- |
| Budowanie klienta | CMake ≥ 3.24, kompilator C/C++ z C++17, Make lub inne narzędzie zgodne z CMake |
| Klient | raylib 5.5, libcurl, nlohmann/json ≥ 3.10, systemowa obsługa wątków |
| Serwer | Go 1.27.0, Gin 1.12.0, pgx/v5 5.9.1 |
| Uruchomienie usług | Docker, Docker Compose, obraz PostgreSQL 18 |
| Zasoby gry | Mapa, sprite’y studenta i goblina oraz czcionka w `client/assets` |

Zależności Go deklaruje `server/go.mod`, a zależności klienta — `client/CMakeLists.txt`. Dockerfile buduje serwer w obrazie `golang:1.27` i kopiuje wynik do obrazu `scratch`.

## Backlog

- Ujednolicenie zasad zakończenia sesji oraz obsługi zdarzeń po jej zakończeniu.
- Zamykanie sesji przy ręcznym zakończeniu rozgrywki.
- Potwierdzanie dostarczenia zdarzeń i ponawianie bez ryzyka podwójnego naliczenia.
- Ranking graczy i endpoint odczytu historii rozgrywek.
- Stopniowe zwiększanie trudności fal oraz nowe typy przeciwników.
- Ulepszenia postaci, przedmioty i efekty dźwiękowe.
- Dokładniejsze dopasowanie przeszkód i punktów pojawiania się goblinów do mapy.
- Migracje schematu bazy i sprawdzanie gotowości usług w Docker Compose.
