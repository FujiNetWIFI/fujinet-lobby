package main

// Retrieve all GameServers with its clients from the database ordered according to 'liveness'
func txGameServerGetAll() (output GameServerClientSlice, err error) {

	// output should be: online first, offline last. Inside each category, newer last ping goes first

	err = DATABASE.Select(&output, "SELECT * FROM GameServerClients ORDER BY Game, Status DESC, Curplayers DESC, Server ")

	if err != nil {
		DB.Printf("%s error: %s", extendedFnName(), err)
		return output, err
	}

	return output, nil
}

// Retrieve GameServers with its clients filtered by platform and appkey (optional) from the database ordered according to 'liveness'
// TODO: we added a simple (and cpu consuming) pagination. It would be better to use a index based, as per:
// https://www2.sqlite.org/cvstrac/wiki?p=ScrollingCursor
// but that would require to change the protocol with the client: they will have to send the latest client received
func txGameServerGetBy(platform string, appkey int, pagesize int, offset int) (output GameServerClientSlice, err error) {

	// LIKE is used to match the platform by partial match. e.g. "spectrum" will match "spectrum48", "spectrum128".
	// Note that SQLite LIKE operator is case-insensitive. It means "A" LIKE "a" is true.
	if appkey == -1 {
		// Sort by Game (so client can efficiently group by game name), Status (so OFFLINE stay at the bottom), Curplayers (so populated servers are at the top), and finally Server name
		err = DATABASE.Select(&output, "SELECT * FROM GameServerClients WHERE client_platform LIKE $1 ORDER BY Game, Status DESC, Curplayers DESC, Server LIMIT $2 OFFSET $3", "%"+platform+"%", pagesize, offset)
	} else {
		err = DATABASE.Select(&output, "SELECT * FROM GameServerClients WHERE client_platform LIKE $1 AND appkey=$2 ORDER BY Status DESC, Lastping DESC LIMIT $3 OFFSET $4", "%"+platform+"%", appkey, pagesize, offset)
	}

	if err != nil {
		DB.Printf("%s error: (%s)", extendedFnName(), err)
		return output, err
	}

	return output, nil
}

// Retrieve one row per game (appkey and name) with its players online, for the games that have
// at least one client on the platform. Games with players online come first, then alphabetical.
// A pagesize of -1 means no limit.
func txGameSummaryGetBy(platform string, pagesize int, offset int) (output GameSummarySlice, err error) {

	// The view has one row per client, so collapse to distinct servers first or a server with
	// several matching clients would be counted (and its players summed) more than once.
	err = DATABASE.Select(&output, `--sql
		SELECT Appkey, Game,
			SUM(CASE WHEN Status = 'online' THEN Curplayers ELSE 0 END) AS players
		FROM (SELECT DISTINCT Serverurl, Appkey, Game, Status, Curplayers FROM GameServerClients WHERE client_platform LIKE $1)
		GROUP BY Appkey, Game
		ORDER BY (players > 0) DESC, Game COLLATE NOCASE, Game, Appkey
		LIMIT $2 OFFSET $3`, "%"+platform+"%", pagesize, offset)

	if err != nil {
		DB.Printf("%s error: (%s)", extendedFnName(), err)
		return output, err
	}

	return output, nil
}

// Retrieve the servers of one game type, filtered by platform: online first, populated servers first.
// A pagesize of -1 means no limit.
func txGameServerListGetBy(platform string, appkey int, pagesize int, offset int) (output GameListSlice, err error) {

	// The view has one row per client, collapse to distinct servers so a server with several matching clients is listed once.
	err = DATABASE.Select(&output, `--sql
		SELECT id, Server, Maxplayers, Curplayers
		FROM (SELECT DISTINCT id, Server, Serverurl, Maxplayers, Curplayers, Status FROM GameServerClients WHERE client_platform LIKE $1 AND appkey = $2)
		ORDER BY Status DESC, Curplayers DESC, Server, Serverurl LIMIT $3 OFFSET $4`, "%"+platform+"%", appkey, pagesize, offset)

	if err != nil {
		DB.Printf("%s error: (%s)", extendedFnName(), err)
		return output, err
	}

	return output, nil
}

// Retrieve the server and client urls for a game and platform from the server with the given id
// (unique within the game type). found is false when there is no such instance.
func txGameInstanceGetBy(platform string, appkey int, id int) (instance GameInstance, found bool, err error) {

	var instances []GameInstance

	err = DATABASE.Select(&instances, "SELECT Serverurl, client_url FROM GameServerClients WHERE client_platform LIKE $1 AND appkey = $2 AND id = $3 ORDER BY Serverurl LIMIT 1", "%"+platform+"%", appkey, id)

	if err != nil {
		DB.Printf("%s error: (%s)", extendedFnName(), err)
		return instance, false, err
	}

	if len(instances) == 0 {
		return instance, false, nil
	}

	return instances[0], true, nil
}

// Upsert new GameServer with client input.
//
// chatUrl is the room URL from the chat service, or empty if the service is
// disabled or did not answer. Empty means "keep whatever is already stored":
// this is a DELETE followed by an INSERT rather than a real upsert, so without
// carrying the old value forward a chat outage would blank the URL on every
// existing server the moment it re-pinged.
func txGameServerUpsert(gs GameServer, chatUrl string) (err error) {

	tx, err := DATABASE.Beginx()

	if err != nil {
		DB.Printf("%s error beginTx: (%s)", extendedFnName(), err)
		tx.Rollback()

		return err
	}

	if chatUrl == "" {
		queryChat := `--sql
			SELECT chat_url FROM GameServer WHERE Serverurl = $1 -- preserve across the delete/insert below
		`
		// A missing row leaves chatUrl empty, which is what we want for a
		// server registering for the first time.
		_ = tx.QueryRow(queryChat, gs.Serverurl).Scan(&chatUrl)
	}

	// Keep the id the server already has, as long as it is still the same game
	// type; otherwise (new server, or moved to another appkey) take the lowest
	// free one. Read it before the delete below wipes the row.
	var id, oldAppkey int
	_ = tx.QueryRow(`SELECT id, appkey FROM GameServer WHERE Serverurl = $1`, gs.Serverurl).Scan(&id, &oldAppkey)

	if id <= 0 || oldAppkey != gs.Appkey {
		id = 0
	}

	queryDelete := `--sql
		DELETE FROM GameServer WHERE Serverurl = $1 -- will delete Clients with DELETE ON CASCADE
	`

	_, err = tx.Exec(queryDelete, gs.Serverurl)

	if err != nil {
		DB.Printf("%s error delete: (%s)", extendedFnName(), err)
		tx.Rollback()

		return err
	}

	if id == 0 {
		id, err = lowestFreeId(tx, gs.Appkey)

		if err != nil {
			DB.Printf("%s error allocating id: (%s)", extendedFnName(), err)
			tx.Rollback()

			return err
		}
	}

	queryInsert := `--sql
		INSERT INTO GameServer (Serverurl, Game, Appkey, Server, Region, Status, Maxplayers, Curplayers, chat_url, id)
		VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10) -- insert main server
	`

	_, err = tx.Exec(queryInsert, gs.Serverurl, gs.Game, gs.Appkey, gs.Server, gs.Region, gs.Status, gs.Maxplayers, gs.Curplayers, chatUrl, id)

	if err != nil {
		DB.Printf("%s error insert GameServer: (%s)", extendedFnName(), err)
		tx.Rollback()

		return err
	}

	queryClient := `--sql
		INSERT INTO Clients (serverurl, client_platform, client_url) VALUES ($1, $2, $3) -- insert each of the clients for the previous server
	`
	for _, client := range gs.Clients {
		_, err = tx.Exec(queryClient, gs.Serverurl, client.Platform, client.Url)

		if err != nil {
			DB.Printf("%s error insert Client: (%s)", extendedFnName(), err)
			tx.Rollback()

			return err
		}

	}

	err = tx.Commit()

	if err != nil {
		DB.Printf("%s error: (%s)", extendedFnName(), err)
		tx.Rollback()

		return err
	}

	return nil
}

// Delete a GameServers with its associated clients
func txGameServerDelete(serverurl string) (err error) {

	query := `--sql
		DELETE FROM GameServer WHERE Serverurl = $1 
	`

	_, err = DATABASE.Exec(query, serverurl)

	if err != nil {
		DB.Printf("%s error: (%s)", extendedFnName(), err)
		return err
	}

	return nil
}

// lowestFreeId returns the smallest id >= 1 not used by any server of the game type,
// so ids freed by deleted servers are reused.
func lowestFreeId(q interface {
	Select(dest interface{}, query string, args ...interface{}) error
}, appkey int) (int, error) {

	var used []int

	err := q.Select(&used, `SELECT id FROM GameServer WHERE appkey = $1 AND id > 0 ORDER BY id`, appkey)
	if err != nil {
		return 0, err
	}

	next := 1
	for _, id := range used {
		if id != next {
			break
		}
		next++
	}

	return next, nil
}

// Give an id to every server that has none (rows from before ids existed),
// per game type, in the order they first registered. Idempotent.
func txGameServerAssignMissingIds() (err error) {

	tx, err := DATABASE.Beginx()
	if err != nil {
		return err
	}

	var missing []struct {
		Serverurl string `db:"serverurl"`
		Appkey    int    `db:"appkey"`
	}

	err = tx.Select(&missing, `SELECT serverurl, appkey FROM GameServer WHERE id <= 0 ORDER BY appkey, rowid`)
	if err != nil {
		tx.Rollback()
		return err
	}

	for _, m := range missing {
		id, err := lowestFreeId(tx, m.Appkey)
		if err == nil {
			_, err = tx.Exec(`UPDATE GameServer SET id = $1 WHERE serverurl = $2`, id, m.Serverurl)
		}

		if err != nil {
			tx.Rollback()
			return err
		}
	}

	return tx.Commit()
}
