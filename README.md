# RingMaBell

Fail-safe C++17 сервер для шкільних дзвоників і сповіщень повітряної тривоги на Linux. Процес працює як `systemd` service, стартує після завантаження ноутбука, перезапускається після падіння і зберігає стан, щоб не дублювати вже програні події.

## Можливості

- Шкільні дзвоники з MP3 за розкладом `config/schedule.csv`.
- Дзвоники автоматично вимкнені у суботу та неділю через `bell_weekends_enabled=false`.
- Повітряна тривога Києва працює 24/7 через Ubilling Aerial Alerts API.
- MP3 для початку тривоги і відбою: `air-alert-start.mp3`, `air-alert-end.mp3`.
- Відновлення після вимкнення світла через BIOS power restore + `systemd`.
- Стан дзвоників: `data/state.csv`.
- Стан повітряної тривоги: `data/air_alert_state.conf`.
- Логи: `logs/ringmabell.log` і `journalctl -u ringmabell`.

## Встановлення На Linux

Рекомендовано Ubuntu Server або Debian без GUI.

```bash
sudo apt update
sudo apt install -y build-essential cmake mpg123 curl alsa-utils
```

Збірка:

```bash
cd /path/to/RingMaBell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Покладіть MP3-файли в `sounds/`:

```text
lesson-start.mp3
lesson-end.mp3
air-alert-start.mp3
air-alert-end.mp3
```

Перевірка перед встановленням:

```bash
./build/ringmabell --config config/settings.conf validate
./build/ringmabell --config config/settings.conf alert-status
./build/ringmabell --config config/settings.conf play-test sounds/lesson-start.mp3
```

Встановлення як systemd service:

```bash
sudo ./scripts/install-linux-systemd.sh
```

Скрипт встановлює файли в `/opt/ringmabell`, створює користувача `ringmabell`, додає його до групи `audio`, створює `/etc/systemd/system/ringmabell.service`, вмикає автозапуск і стартує сервіс.

Перегляд статусу:

```bash
sudo systemctl status ringmabell
sudo journalctl -u ringmabell -f
```

Видалення service:

```bash
sudo ./scripts/uninstall-linux-systemd.sh
```

## Налаштування Звуку

За замовчуванням `audio_backend=auto`, на Linux він віддає перевагу `mpg123`.

Якщо звук не грає з `systemd`, зафіксуйте ALSA-команду в `/opt/ringmabell/config/settings.conf`:

```ini
audio_backend=command
audio_command=/usr/bin/mpg123 -q -o alsa {file}
```

Перевірка аудіопристроїв:

```bash
aplay -l
speaker-test -t wav -c 2
```

Якщо потрібен конкретний аудіовихід, можна вказати ALSA device:

```ini
audio_command=/usr/bin/mpg123 -q -o alsa -a hw:0,0 {file}
```

## Повітряні Тривоги Києва

API:

```text
https://ubilling.net.ua/aerialalerts/
```

Токен не потрібен. Сервер читає `states["м. Київ"].alertnow`.

Polling за замовчуванням кожні `3` секунди. Це найшвидший практичний режим для цього REST API: у документації Ubilling вказаний кеш сирих даних близько 3 секунд і ліміт 2 запити/сек з одного хоста.

Важливо: це локальне дублююче сповіщення для будівлі, а не заміна офіційної системи оповіщення, сирен, ДСНС, КМДА або застосунку `Київ Цифровий`.

## Відновлення Після Світла

Практична схема:

1. У BIOS/UEFI ввімкнути `Restore on AC Power Loss`, `AC Back`, `Power On After Power Fail` або `Last State`.
2. Linux завантажується без входу користувача.
3. `systemd` запускає `ringmabell.service`.
4. Якщо процес падає, `systemd` перезапускає його через 5 секунд.
5. Якщо мережі ще немає, шкільні дзвоники все одно працюють; повітряна тривога почне оновлюватись після появи інтернету.

Після старту RingMaBell читає `data/state.csv`. Уже програні дзвоники за поточний день не дублюються. Якщо світло повернулось протягом `missed_grace_seconds` після пропущеного дзвоника, він дограється; старіші пропущені дзвоники позначаються як `skipped`.

Повітряна тривога читає `data/air_alert_state.conf`. Якщо тривога почалась, поки ноут був вимкнений, після запуску система побачить `active` і програє `air-alert-start.mp3`. Якщо стан уже був `active`, простий рестарт не створить повторний сигнал.

Рекомендовано вимкнути sleep/hibernate:

```bash
sudo systemctl mask sleep.target suspend.target hibernate.target hybrid-sleep.target
```

Для ноутбука також бажано налаштувати ігнорування закриття кришки в `/etc/systemd/logind.conf`:

```ini
HandleLidSwitch=ignore
HandleLidSwitchExternalPower=ignore
```

Після зміни:

```bash
sudo systemctl restart systemd-logind
```
