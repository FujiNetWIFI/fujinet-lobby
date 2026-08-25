# FujiNet Lobby Server

The FujiNet Lobby Server is a lightweight game server registry service written in Go. It allows game servers to register themselves and clients to discover available game servers. The service is designed to be compatible with 8-bit systems like Atari.

## Overview

Version: 5.5.7

This service acts as a central registry for the FujiNet Game System, allowing:
- Game servers to register their availability
- Clients to discover active game servers
- Support for multiple platforms through client-specific endpoints
- Optimized data formats for 8-bit systems

## Core Components

### Data Model

- **GameServer**: Represents a game server with properties like game name, server name, region, status, player counts, etc.
- **GameClient**: Represents client platforms that can connect to a game server
- **GameServerMin**: A minimized version of GameServer optimized for 8-bit clients with shortened field names and binary serialization support

### API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | HTML view of available servers |
| `/docs` | GET | Documentation page |
| `/viewFull` | GET | Full JSON representation of all servers |
| `/view` | GET | Minimized JSON or binary representation of servers (optimized for 8-bit clients) |
| `/version` | GET | Server version and status information |
| `/server` | POST | Register or update a server |
| `/server` | DELETE | Remove a server from the registry |

### Database

- Uses SQLite with Write-Ahead Logging (WAL) for performance
- Tables include:
  - `GameServer` - Stores server information
  - `Clients` - Stores client platform information linked to servers
  - `GameServerClients` - A view joining the two tables

## Key Features

1. **Binary Format Support**: Can return server data in binary format optimized for 8-bit clients
2. **Pagination**: Supports paginated results for clients with limited memory
3. **Filtering**: Can filter servers by platform and application key
4. **Webhook Integration**: Can notify an event server when servers are added/updated/removed
5. **HTML Interface**: Provides a human-readable web interface for browsing servers

## Technical Details

- Written in Go using the Gin web framework
- Uses a scheduler for background tasks
- Implements proper signal handling for clean shutdown
- Includes validation for input data with detailed error messages

## How It Works

1. Game servers register themselves via the POST /server endpoint
2. The service stores this information in the SQLite database
3. Clients can query available servers via various endpoints
4. The service can optionally notify an event server via webhook when changes occur
5. Servers are automatically sorted by online status and player count

## Running the Server

```bash
# Basic usage
./server -srvaddr :8080

# With webhook for event notifications
./server -srvaddr :8080 -evtaddr http://event-server/webhook

# Show version
./server -version

# Show help
./server -help
```

## Command Line Options

- `-srvaddr`: HTTP server address and port (default ":8080")
- `-evtaddr`: Event server webhook URL
- `-prod`: Enable production mode (see below)
- `-trustedproxy`: IP or CIDR of a reverse proxy allowed to set `X-Forwarded-For` (may be repeated)
- `-version`: Show current version
- `-help`: Show help information

## Environment Variables

- `LOG_LEVEL=PROD`: Disables debug logging and enables production mode

## Production Mode

Production mode is enabled by the `-prod` flag or by `LOG_LEVEL=PROD`. When it is
on, `POST /server` rejects registrations involving addresses that are not reachable
from the public internet:

- **HTTP 403** if the request itself arrives from a non-routable address
- **HTTP 400** if the `serverurl`, or any client `url`, points at a non-routable host

Non-routable covers private ranges (10/8, 172.16/12, 192.168/16, fc00::/7),
loopback, link-local, carrier-grade NAT (100.64/10), multicast, the reserved and
documentation ranges, and the names `localhost`, `*.local`, `*.internal`, `*.lan`
and `*.home.arpa`. Hostnames are never resolved via DNS — only literal addresses
and those local-only suffixes are matched.

Without production mode the server behaves exactly as before, so a lobby run
locally against a LAN game server keeps working.

### Running behind a reverse proxy

`c.ClientIP()` is derived from `X-Forwarded-For`, which Gin trusts from any source
by default. If the lobby sits behind a reverse proxy, pass `-trustedproxy` with the
proxy's address so the source check reads the real client address and cannot be
bypassed with a forged header:

```bash
LOG_LEVEL=PROD ./lobbyPersist -srvaddr :8080 -trustedproxy 10.0.0.0/8
```

The proxy must set `X-Forwarded-For`. If it does not, `c.ClientIP()` resolves to
the proxy's own address, and a proxy on the same LAN would cause every registration
to be rejected.

## Database Schema

The server uses SQLite with Write-Ahead Logging (WAL) for improved performance. The database schema consists of the following:

### Tables

#### GameServer
Stores information about each game server:

| Column | Type | Description |
|--------|------|-------------|
| serverurl | TEXT | Primary key - Unique URL of the server |
| game | TEXT | Name of the game |
| appkey | INT | Application key for the game |
| server | TEXT | Server name |
| region | TEXT | Geographic region code |
| status | TEXT | Server status ("online" or "offline") |
| maxplayers | INT | Maximum number of players allowed |
| curplayers | INT | Current number of players |
| lastping | DATETIME | Timestamp of last server ping (auto-updated) |

#### Clients
Stores client platform information for each server:

| Column | Type | Description |
|--------|------|-------------|
| serverurl | TEXT | Foreign key to GameServer.serverurl |
| client_platform | TEXT | Platform identifier (e.g., "atari", "apple2") |
| client_url | TEXT | URL for the client software |

### Views

#### GameServerClients
A view that joins GameServer and Clients tables:
```sql
CREATE VIEW GameServerClients AS 
SELECT GameServer.*, 
       Clients.client_platform as client_platform, 
       Clients.client_url as client_url 
FROM GameServer 
JOIN Clients ON GameServer.Serverurl = Clients.Serverurl;
```

### Indexes

- `idx_GameServer_Serverurl`: Index on GameServer.Serverurl
- `idx_Clients_Serverurl`: Index on Clients.Serverurl

### Relationships

- The `Clients` table has a foreign key relationship to `GameServer` with CASCADE DELETE, meaning when a server is deleted, all associated client records are automatically removed.
