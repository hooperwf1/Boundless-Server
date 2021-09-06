# Boundless

This is server software for a project that aims to provide both a text, visual, and voice chat service to users.

## Client

An official client has not yet been released but you may use telnet or similar to connect and send messages.
Simply connect to either Boundless.Chat at port 6667 or another custom instance.

## Installation

**Note:** This is currently untested for both MacOS and Windows.  
Make sure that you have installed OpenSSL and SQLite.

Debian/Ubuntu:
```sh
apt install libssl-dev sqlite3 libsqlite3-dev
```

Arch Linux (Normally installed by default):
```sh
pacman -S openssl sqlite
```

Fedora:
```sh
dnf install openssl sqlite sqlite-devel sqlite-tcl sqlite-jdbc
```

Compilation:
```sh
git clone https://github.com/hooperwf1/Boundless-Server.git
cd Boundless-Server
sudo make install
boundlessd
```

## Configuration

The default configuration file can be found at `/etc/boundless-server/settings.conf`.

## Help

For more information you may visit the website on [Boundless.Chat](http://Boundless.Chat) or send a message to #help in the Boundless.Chat server.
