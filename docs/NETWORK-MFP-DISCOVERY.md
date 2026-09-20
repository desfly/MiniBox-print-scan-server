# MiniBox MFP zero-configuration network contract

## Product rule
A user must never need to know the MiniBox IP address or manually create a RAW/9100 port for normal operation.

The HP LaserJet M1522n is connected by USB to MiniBox. MiniBox is a Wi-Fi client on the user's LAN. Ethernet is not the primary MFP network channel. Printing and scanning are exposed on the Wi-Fi LAN using standard service discovery.

## Required discovery
* mDNS/DNS-SD on UDP 5353.
* `_ipp._tcp` for driverless network printing at `/ipp/print`.
* `_printer._tcp` may be advertised as a compatibility alias, but RAW/9100 is not the installation UX.
* `_uscan._tcp` for eSCL/AirScan scanner discovery at `/eSCL`.
* Stable service instance identity must be derived from the MiniBox device identity, not its DHCP address.

## Client acceptance criteria
### Windows
The M1522 must appear in Add printer or scanner without entering an IP address. A DHCP address change must not require reinstalling the printer.

### Android
A phone on the same Wi-Fi must discover the printer through the platform/Mopria IPP path and submit supported jobs without entering an IP address. Scanning applications supporting eSCL/AirScan must discover the scanner.

### Apple
An iPhone/iPad/Mac on the same Wi-Fi must discover the print service using Bonjour/AirPrint DNS-SD records. Scanner discovery uses `_uscan._tcp`/eSCL where supported.

## MiniBox implementation constraints
* USB access to the M1522 remains userspace/libusb.
* `usblp` / `kmod-usb-printer` remains forbidden.
* CUPS is not introduced.
* The existing raw print transport can remain as a diagnostic/compatibility endpoint, but is not the normal installation path.
* IPP and eSCL front ends terminate on MiniBox and bridge to the existing libusb print/scan back ends.

## Runtime architecture
`minibox-discoveryd` publishes DNS-SD services only while the corresponding local service is healthy. `ippd` accepts IPP jobs, validates attributes and streams supported document data into `printerd`. `escld` exposes scanner capabilities/status and streams scan jobs through `scand`. All daemons are supervised by procd.

## Hardware target
HP LaserJet M1522n, USB VID:PID `03f0:4517`.
