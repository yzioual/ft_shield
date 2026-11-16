# ft_shield

A minimal Linux background daemon and remote administration tool.

## Features

- **Persistence**: Automatically installs itself as a `systemd` service (`/etc/systemd/system/ft_shield.service`).
- **Daemon Process**: Runs in the background with single-instance locking (`/var/lock/ft_shield.lock`).
- **Authentication**: Keycode verification using MD5 hashing baked in at compile time.
- **Remote Access**: TCP server listening on port `4242` (supports up to 3 concurrent clients).
- **Remote Shell**: Ability to spawn an interactive root `/bin/bash` shell session over the wire.

## Prerequisites

- Linux (x86_64) with `systemd`
- GCC & Make
- Root privileges

## Build

Compile by specifying an authentication password:

```bash
make PASSWORD="your_password"
```

## Usage

### 1. Installation

Execute the binary with root privileges. It copies itself to `/bin/ft_shield`, sets up the systemd service, and starts the daemon:

```bash
sudo ./ft_shield
```

### 2. Connect

Connect using Netcat:

```bash
nc 127.0.0.1 4242
```

Authenticate by entering the password:

```text
Keycode: your_password
$> ?
? show help
shell Spawn remote shell on 4242
```

### 3. Remote Shell

Send `shell` to arm shell mode:

```text
$> shell
Spawning shell on port 4242
```

Open a new connection on port 4242 to get an interactive root shell:

```bash
nc 127.0.0.1 4242
```

## Management & Removal

Stop and uninstall the service:

```bash
sudo systemctl stop ft_shield
sudo systemctl disable ft_shield
sudo rm -f /etc/systemd/system/ft_shield.service /bin/ft_shield /var/lock/ft_shield.lock
sudo systemctl daemon-reload
```
