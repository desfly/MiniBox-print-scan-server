# MiniBox Print/Scan Server — PROJECT CONTRACT

> **ЦЕЙ КОНТРАКТ ЧИТАТИ ПЕРЕД БУДЬ-ЯКОЮ ЗМІНОЮ ПРОШИВКИ, МЕРЕЖІ, DISCOVERY АБО UPDATE/RESTORE.**

## 1. Незмінна штатна логіка MiniBox

**POWER ON → OpenWrt → автоматичне підключення як Wi-Fi STA до материнської Wi-Fi → робочий IP у цій мережі → перевірка наявності USB MFP → визначення HP LaserJet M1522n (USB 03f0:4517) → запуск і health-check printer/scanner → публікація маркерів автопошуку → Windows та Android автоматично бачать MFP → PRINT / SCAN.**

- MiniBox працює як Wi-Fi client/STA, не потребуючи відкритої Web UI.
- Printer: IPP, `/ipp/print`, TCP 631.
- Scanner: eSCL, `/eSCL`, TCP 8080.
- Discovery: DNS-SD/mDNS для printer/scanner + WSD для Windows; Android також має автоматично знаходити MFP.
- `discoveryd` **не має права змінювати Wi-Fi/network configuration**. Він лише читає фактичний активний інтерфейс/IP та публікує сервіси.
- MFP unplugged: MiniBox і Web UI залишаються доступними, але не рекламують неготовий MFP як справний.
- MFP plugged back: USB hot-plug автоматично визначає M1522n, перевіряє printer/scanner і відновлює discovery без reboot.
- Web UI повинна показувати окремо: MFP USB present/missing, printer ready/error, scanner ready/error, discovery ready/error.
- MiniWeb/LuCI — керування та діагностика; вони не є умовою роботи PRINT/SCAN.

## 2. НІКОЛИ НЕ СТИРАТИ РОБОЧІ НАЛАШТУВАННЯ

**ЗАБОРОНЕНО під час звичайного update/restore стирати, скидати або мовчки перезаписувати робочу конфігурацію MiniBox.**

Особливо зберігати:
- `/etc/config/network`
- `/etc/config/wireless` (SSID/password/STA)
- `/etc/config/firewall`
- `/etc/config/dhcp`
- `/etc/config/uhttpd`
- MiniBox/MFP configuration and identity

Rules:
- **NEVER use `sysupgrade -n` for a normal update/restore.**
- No factory reset unless the user explicitly requests it after a backup.
- Before any destructive operation: make and verify a backup first.
- Update/restore must preserve the existing working Wi-Fi STA credentials and network addresses.
- Recovery/default scripts may initialize missing/default settings only; they must not overwrite an already configured working network.
- No daemon, including discovery, printer or scanner services, may rewrite Wi-Fi credentials.
- Firmware changes must not alter these invariants without an explicit deliberate contract change.

## 3. Recovery network invariant

Known target configuration:
- Wi-Fi working address: `192.168.55.250/24`
- Ethernet/recovery address: `192.168.55.251/24`

The Ethernet recovery address is a fallback. It must not destroy a configured Wi-Fi STA setup.

## 4. Definition of done

A build is not considered restored/working until hardware testing confirms:
1. reboot → automatic Wi-Fi connection;
2. HP LaserJet M1522n detected over USB;
3. Web UI reports MFP/printer/scanner/discovery status;
4. Windows automatically discovers the printer and scanner;
5. Android automatically discovers the MFP;
6. real printing works;
7. real scanning works;
8. reboot/update preserves all working configuration.

**If a proposed change conflicts with sections 1 or 2, stop: the change is wrong for this project.**
