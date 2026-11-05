package main

import (
	"database/sql"

	"github.com/jmoiron/sqlx"
	_ "github.com/mattn/go-sqlite3"
)

type lobbyDB struct {
	*sqlx.DB

	// add Errors https://pkg.go.dev/github.com/mattn/go-sqlite3@v1.14.14#ErrIoErr
}

func (db *lobbyDB) Get(dest interface{}, query string, args ...interface{}) (err error) {

	err = db.DB.Get(dest, query, args...)

	if err != sql.ErrNoRows {
		return err
	}

	return nil
}

func (db *lobbyDB) In(query string, args ...interface{}) (string, []interface{}, error) {
	return sqlx.In(query, args...)
}

func (db *lobbyDB) SelectIn(dest interface{}, query string, args ...interface{}) (err error) {

	qry, inargs, err := DATABASE.In(query, args...)
	if err != nil {
		return err
	}

	return db.DB.Select(dest, qry, inargs...)
}

func (db *lobbyDB) ExecIn(query string, args ...interface{}) (res sql.Result, err error) {

	qry, inargs, err := DATABASE.In(query, args...)
	if err != nil {
		return res, err
	}

	return db.DB.Exec(qry, inargs...)

}

func init_db() {
	DATABASE = &lobbyDB{DB: sqlx.MustConnect("sqlite3", "db/lobby.sqlite3?_foreign_keys=on&_journal=WAL&_timeout=300")}

	DB.Println("Connected to lobby.sqlite3")

	// https://dev.to/lefebvre/speed-up-sqlite-with-write-ahead-logging-wal-do

	// Configure Write Ahead Log
	_, err := DATABASE.Exec(`PRAGMA busy_timeout=300;PRAGMA journal_mode=WAL;PRAGMA foreign_keys=ON;`)

	if err != nil {
		DB.Fatalf("Unable to set PRAGMA correctly (%s)", err)
	}

	migrate_db()
}

// migrate_db brings an existing database up to the current schema.
//
// There is no migration framework here: lobby_schema.sql builds a fresh
// database and `make install-db` drops whatever was there. That is fine for a
// new deployment and useless for a running one, so additive changes are applied
// here instead, idempotently, at every startup.
func migrate_db() {

	migrate_add_column("chat_url", "TEXT NOT NULL DEFAULT ''")
	migrate_add_column("id", "INT NOT NULL DEFAULT 0")

	// Servers stored before ids existed (or before this process ran the
	// migration) have id 0; number them now.
	err := txGameServerAssignMissingIds()
	if err != nil {
		DB.Fatalf("Unable to assign server ids (%s)", err)
	}
}

// migrate_add_column adds a column to GameServer if it is not there yet.
func migrate_add_column(name string, definition string) {

	// SQLite has no "ADD COLUMN IF NOT EXISTS", so ask before telling.
	var columns []struct {
		Name string `db:"name"`
	}

	err := DATABASE.Select(&columns, `SELECT name FROM pragma_table_info('GameServer')`)
	if err != nil {
		DB.Fatalf("Unable to inspect the GameServer schema (%s)", err)
	}

	for _, column := range columns {
		if column.Name == name {
			return
		}
	}

	_, err = DATABASE.Exec(`ALTER TABLE GameServer ADD COLUMN ` + name + ` ` + definition)
	if err != nil {
		DB.Fatalf("Unable to add the %s column (%s)", name, err)
	}

	// The GameServerClients view selects GameServer.*, which SQLite expands
	// when the view is used rather than when it was created -- but only after
	// the schema cache is reloaded. Recreate it so the new column is visible to
	// this process immediately.
	_, err = DATABASE.Exec(`
		DROP VIEW IF EXISTS GameServerClients;
		CREATE VIEW GameServerClients AS
			SELECT GameServer.*, Clients.client_platform as client_platform, Clients.client_url as client_url
			FROM GameServer JOIN Clients ON GameServer.Serverurl = Clients.Serverurl;`)
	if err != nil {
		DB.Fatalf("Unable to rebuild the GameServerClients view (%s)", err)
	}

	DB.Printf("Migrated: added GameServer.%s", name)
}
