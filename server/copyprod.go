package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"
)

const PRODUCTION_VIEWFULL = "https://lobby.fujinet.online/viewFull"

// copy_production_db replaces the servers in the local database with a snapshot
// of production, so local development sees what players see.
//
// This is a command line action (-copyprod), not an endpoint: nothing that can
// be reached over http is able to wipe the database.
func copy_production_db(url string) error {

	INFO.Printf("fetching servers from %s", url)

	servers, err := fetchGameServers(url, 15*time.Second)
	if err != nil {
		return err
	}

	// Only clear once we hold a usable snapshot: a failed or empty fetch must
	// leave the local data as it was.
	if _, err := DATABASE.Exec("DELETE FROM GameServer"); err != nil { // Clients go with it, ON DELETE CASCADE
		return fmt.Errorf("unable to clear the local database (%w)", err)
	}

	for _, server := range servers {
		// Keep the chat url production assigned rather than asking the chat service for one
		if err := txGameServerUpsert(server, server.ChatUrl); err != nil {
			return fmt.Errorf("unable to store %s (%w)", server.Serverurl, err)
		}
	}

	INFO.Printf("local database now mirrors production: %d servers", len(servers))

	return nil
}

// fetchGameServers reads the /viewFull json from a lobby.
func fetchGameServers(url string, timeout time.Duration) (servers GameServerSlice, err error) {

	client := &http.Client{Timeout: timeout}

	resp, err := client.Get(url)
	if err != nil {
		return nil, fmt.Errorf("unable to reach %s (%w)", url, err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("%s answered HTTP %d", url, resp.StatusCode)
	}

	body, err := io.ReadAll(io.LimitReader(resp.Body, 16<<20))
	if err != nil {
		return nil, fmt.Errorf("unable to read the answer from %s (%w)", url, err)
	}

	if err := json.Unmarshal(body, &servers); err != nil {
		return nil, fmt.Errorf("%s did not answer a list of servers (%w)", url, err)
	}

	if len(servers) == 0 {
		return nil, fmt.Errorf("%s has no servers, refusing to empty the local database", url)
	}

	return servers, nil
}
