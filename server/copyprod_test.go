package main

import (
	"net/http"
	"net/http/httptest"
	"testing"
	"time"
)

func TestFetchGameServersRejectsBadAnswers(t *testing.T) {

	for name, handler := range map[string]http.HandlerFunc{
		"error status": func(w http.ResponseWriter, r *http.Request) { http.Error(w, "no", http.StatusNotFound) },
		"not a list":   func(w http.ResponseWriter, r *http.Request) { w.Write([]byte(`{"success":false}`)) },
		"empty list":   func(w http.ResponseWriter, r *http.Request) { w.Write([]byte(`[]`)) },
	} {
		srv := httptest.NewServer(handler)

		if _, err := fetchGameServers(srv.URL, time.Second); err == nil {
			t.Errorf("%s: expected an error", name)
		}

		srv.Close()
	}
}

func TestCopyProductionDB(t *testing.T) {

	t.Cleanup(func() { DATABASE.Exec("DELETE FROM GameServer") })

	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte(`[{"game":"Super Chess","appkey":3,"server":"chess.rogersm.net","region":"eu","serverurl":"http://chess.rogersm.net/server","status":"online","maxplayers":2,"curplayers":1,"chaturl":"https://q.example/abc","clients":[{"platform":"atari","url":"http://chess.rogersm.net/atarichess.xex"},{"platform":"c64","url":"http://chess.rogersm.net/c64chess.xex"}]}]`))
	}))
	defer srv.Close()

	// a leftover that must not survive the copy
	if err := txGameServerUpsert(GameServers1(), ""); err != nil {
		t.Fatal(err)
	}

	if err := copy_production_db(srv.URL); err != nil {
		t.Fatal(err)
	}

	got, err := txGameServerGetAll()
	if err != nil {
		t.Fatal(err)
	}

	if len(got) != 2 || got[0].Server != "chess.rogersm.net" || got[0].Chat_url != "https://q.example/abc" {
		t.Errorf("local database does not mirror the source: %+v", got)
	}

	// an unusable source leaves what is there alone
	bad := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { w.Write([]byte(`[]`)) }))
	defer bad.Close()

	if err := copy_production_db(bad.URL); err == nil {
		t.Error("expected an error from an empty source")
	}

	if got, _ := txGameServerGetAll(); len(got) != 2 {
		t.Errorf("a failed copy changed the local database: %+v", got)
	}
}

func GameServers1() GameServer {
	return GameServer{Game: "Old", Appkey: 9, Server: "old", Region: "us", Serverurl: "http://old/server", Status: "online", Maxplayers: 2,
		Clients: []GameClient{{Platform: "atari", Url: "http://old/a.xex"}}}
}
