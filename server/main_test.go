package main

import (
	"bytes"
	"fmt"
	"log"
	"net/http"
	"net/http/httptest"
	"os"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/jmoiron/sqlx"
	"github.com/nsf/jsondiff" // TODO: can we use some core golang functionality?
)

var ROUTER = setupRouter()

func TestMain(m *testing.M) {

	DATABASE = &lobbyDB{DB: sqlx.MustConnect("sqlite3", "db/lobby.sqlite3?_foreign_keys=on")}

	DATABASE.Exec("DELETE FROM GameServer")
	DB = NewCustomLogger("db", "\u001b[36mDB: \u001B[0m", log.LstdFlags)
	DB.SetActive(false) // we don't want the DB logger to pollute the test
	DEBUG = NewCustomLogger("debug", "\u001b[36mDEBUG: \u001B[0m", log.LstdFlags)
	DEBUG.SetActive(false)

	// handlers log through these, an uninitialised one is a nil *log.Logger and
	// panics the request rather than failing the assertion we care about
	INFO = NewCustomLogger("info", "INFO: ", log.LstdFlags)
	INFO.SetActive(false)
	WARN = NewCustomLogger("warn", "WARN: ", log.LstdFlags)
	WARN.SetActive(false)
	ERROR = NewCustomLogger("error", "ERROR: ", log.LstdFlags)
	ERROR.SetActive(false)

	// Run the same additive migrations init_db() applies at startup. Has to
	// come after the loggers, which migrate_db() writes through. The local
	// db/lobby.sqlite3 is whatever a previous `make install-db` left behind, so
	// this doubles as coverage that the migration works against a database
	// built from an older schema.
	migrate_db()

	os.Exit(m.Run())
}

var GameServersIn = []string{
	`{
        "game": "Super Chess",
        "appkey": 3,
        "server": "chess.rogersm.net",
        "serverurl": "http://chess.rogersm.net/server",
        "region": "eu",
        "status": "online",
        "maxplayers": 2,
        "curplayers": 1,
        "clients": [
            {"platform":"atari", "url":"http://chess.rogersm.net/atarichess.xex" },
            {"platform": "spectrum", "url":"http://chess.rogersm.net/speccychess.xex"},
            {"platform": "c64", "url":"http://chess.rogersm.net/c64chess.xex"}

        ]
    }`,
	`{
        "game": "Battleship",
        "appkey": 2,
        "region": "au",
        "server": "8bitBattleship.com",
        "serverurl": "https://8bitBattleship.com/battlebots",
        "status": "online",
        "maxplayers": 2,
        "curplayers": 1,
        "clients": [
            {"platform":"atari", "url":"https://8bitBattleship.com/atariship.xex" },
            {"platform": "spectrum", "url":"https://8bitBattleship.com/specship.xex"},
            {"platform": "c64", "url":"https://8bitBattleship.com/c64ship.xex"},
            {"platform": "amiga", "url":"https://8bitBattleship.com/amigaship.xex"}
        ]
	}`,
	`{
        "game": "5 CARD STUD",
        "appkey": 1,
        "region": "us",
        "server": "erichomeserver.com",
        "serverurl": "tcp://thomcorner.com/pokerbots",
        "status": "online",
        "maxplayers": 8,
        "curplayers": 1,
        "clients": [
            {"platform":"atari", "url":"tcp://thomcorner.com/clientus/ataripoker.xex" },
            {"platform": "spectrum", "url":"tcp://thomcorner.com/clientus/specpoker.xex"},
            {"platform": "c64", "url":"tcp://thomcorner.com/clientus/c64poker.xex"},
            {"platform": "lynx", "url":"tcp://thomcorner.com/clientus/lynxpoker.xex"}
        ]
	}`,
	`{
        "game": "Battleship",
        "appkey": 2,
        "region": "AP",
        "server": "8bitBattleship.com",
        "serverurl": "https://8bitBattleship.com/battlehuman",
        "status": "online",
        "maxplayers": 2,
        "curplayers": 0,
        "clients": [
            {"platform":"atari", "url":"https://8bitBattleship.com/atariship.xex" },
            {"platform": "spectrum", "url":"https://8bitBattleship.com/specship.xex"},
            {"platform": "c64", "url":"https://8bitBattleship.com/c64ship.xex"},
            {"platform": "amiga", "url":"https://8bitBattleship.com/amigaship.xex"},
            {"platform": "vic20", "url":"https://8bitBattleship.com/vic20ship.xex"}

        ]
    }`,
	`{
        "game": "5 CARD STUD",
        "appkey": 1,
        "region": "AL",
        "server": "erichomeserver.com",
        "serverurl": "tcp://thomcorner.com/server5",
        "status": "offline",
        "maxplayers": 3,
        "curplayers": 0,
        "clients": [
            {"platform":"atari", "url":"tcp://thomcorner.com/ataripoker.xex" },
            {"platform": "spectrum", "url":"tcp://thomcorner.com/specpoker.xex"},
            {"platform": "c64", "url":"tcp://thomcorner.com/c64poker.xex"},
            {"platform": "amiga", "url":"tcp://thomcorner.com/amigapoker.xex"}
        ]
    }`,
	`{
        "game": "5 CARD STUD",
        "appkey": 1,
        "region": "VA",
        "server": "thomcorner.com",
        "serverurl": "tcp://thomcorner.com/pokerhuman",
        "status": "online",
        "maxplayers": 8,
        "curplayers": 4,
        "clients": [
            {"platform":"atari", "url":"tcp://thomcorner.com/clt/ataripoker.xex" }  
        ]
    }`}

var GameServersOut = `"[
    {
        "game": "5 CARD STUD",
        "appkey": 1,
        "server": "thomcorner.com",
        "region": "vatican",
        "serverurl": "tcp://thomcorner.com/pokerhuman",
        "status": "online",
        "maxplayers": 8,
        "curplayers": 4,
        "clients": [
            {
                "platform": "atari",
                "url": "tcp://thomcorner.com/clt/ataripoker.xex"
            }
        ]
    },
    {
        "game": "Battleship",
        "appkey": 2,
        "server": "8bitBattleship.com",
        "region": "apac",
        "serverurl": "https://8bitBattleship.com/battlehuman",
        "status": "online",
        "maxplayers": 2,
        "curplayers": 0,
        "clients": [
            {
                "platform": "atari",
                "url": "https://8bitBattleship.com/atariship.xex"
            },
            {
                "platform": "spectrum",
                "url": "https://8bitBattleship.com/specship.xex"
            },
            {
                "platform": "c64",
                "url": "https://8bitBattleship.com/c64ship.xex"
            },
            {
                "platform": "amiga",
                "url": "https://8bitBattleship.com/amigaship.xex"
            },
            {
                "platform": "vic20",
                "url": "https://8bitBattleship.com/vic20ship.xex"
            }
        ]
    },
    {
        "game": "5 CARD STUD",
        "appkey": 1,
        "server": "erichomeserver.com",
        "region": "us",
        "serverurl": "tcp://thomcorner.com/pokerbots",
        "status": "online",
        "maxplayers": 8,
        "curplayers": 1,
        "clients": [
            {
                "platform": "atari",
                "url": "tcp://thomcorner.com/clientus/ataripoker.xex"
            },
            {
                "platform": "spectrum",
                "url": "tcp://thomcorner.com/clientus/specpoker.xex"
            },
            {
                "platform": "c64",
                "url": "tcp://thomcorner.com/clientus/c64poker.xex"
            },
            {
                "platform": "lynx",
                "url": "tcp://thomcorner.com/clientus/lynxpoker.xex"
            }
        ]
    },
    {
        "game": "Battleship",
        "appkey": 2,
        "server": "8bitBattleship.com",
        "region": "au",
        "serverurl": "https://8bitBattleship.com/battlebots",
        "status": "online",
        "maxplayers": 2,
        "curplayers": 1,
        "clients": [
            {
                "platform": "atari",
                "url": "https://8bitBattleship.com/atariship.xex"
            },
            {
                "platform": "spectrum",
                "url": "https://8bitBattleship.com/specship.xex"
            },
            {
                "platform": "c64",
                "url": "https://8bitBattleship.com/c64ship.xex"
            },
            {
                "platform": "amiga",
                "url": "https://8bitBattleship.com/amigaship.xex"
            }
        ]
    },
    {
        "game": "Super Chess",
        "appkey": 3,
        "server": "chess.rogersm.net",
        "region": "eu",
        "serverurl": "http://chess.rogersm.net/server",
        "status": "online",
        "maxplayers": 2,
        "curplayers": 1,
        "clients": [
            {
                "platform": "atari",
                "url": "http://chess.rogersm.net/atarichess.xex"
            },
            {
                "platform": "spectrum",
                "url": "http://chess.rogersm.net/speccychess.xex"
            },
            {
                "platform": "c64",
                "url": "http://chess.rogersm.net/c64chess.xex"
            }
        ]
    },
    {
        "game": "5 CARD STUD",
        "appkey": 1,
        "server": "erichomeserver.com",
        "region": "all",
        "serverurl": "tcp://thomcorner.com/server5",
        "status": "offline",
        "maxplayers": 3,
        "curplayers": 0,
        "clients": [
            {
                "platform": "atari",
                "url": "tcp://thomcorner.com/ataripoker.xex"
            },
            {
                "platform": "spectrum",
                "url": "tcp://thomcorner.com/specpoker.xex"
            },
            {
                "platform": "c64",
                "url": "tcp://thomcorner.com/c64poker.xex"
            },
            {
                "platform": "amiga",
                "url": "tcp://thomcorner.com/amigapoker.xex"
            }
        ]
    }
]`

var GameServersOutMin = `[{"g":"Battleship","t":2,"u":"https://8bitBattleship.com/battlehuman","c":"https://8bitBattleship.com/specship.xex","s":"8bitBattleship.com","r":"apac","o":1,"m":2,"p":0,"a":0},{"g":"5 CARD STUD","t":1,"u":"tcp://thomcorner.com/pokerbots","c":"tcp://thomcorner.com/clientus/specpoker.xex","s":"erichomeserver.com","r":"us","o":1,"m":8,"p":1,"a":0},{"g":"Battleship","t":2,"u":"https://8bitBattleship.com/battlebots","c":"https://8bitBattleship.com/specship.xex","s":"8bitBattleship.com","r":"au","o":1,"m":2,"p":1,"a":0},{"g":"Super Chess","t":3,"u":"http://chess.rogersm.net/server","c":"http://chess.rogersm.net/speccychess.xex","s":"chess.rogersm.net","r":"eu","o":1,"m":2,"p":1,"a":0},{"g":"5 CARD STUD","t":1,"u":"tcp://thomcorner.com/server5","c":"tcp://thomcorner.com/specpoker.xex","s":"erichomeserver.com","r":"all","o":0,"m":3,"p":0,"a":0}]`
var GameServersOutMinAppKey1 = `[{"g":"5 CARD STUD","t":1,"u":"tcp://thomcorner.com/pokerbots","c":"tcp://thomcorner.com/clientus/specpoker.xex","s":"erichomeserver.com","r":"us","o":1,"m":8,"p":1,"a":0},{"g":"5 CARD STUD","t":1,"u":"tcp://thomcorner.com/server5","c":"tcp://thomcorner.com/specpoker.xex","s":"erichomeserver.com","r":"AL","o":0,"m":3,"p":0,"a":0}]`

func setupRouter() *gin.Engine {

	router := gin.Default()

	router.GET("/viewFull", ShowServers)
	router.GET("/view", ShowServersMinimised)
	router.GET("/games", ShowGamesSummary)
	router.GET("/games/:game", ShowGamesList)
	router.GET("/games/:game/:server", ShowGamesInstance)
	router.POST("/server", UpsertServer)
	router.GET("/version", ShowStatus)

	return router
}

func assertHTTPAnswerJSON(w *httptest.ResponseRecorder, HTTPCode int, HTTPBody string) (err []error) {
	if w.Code != HTTPCode {
		err = append(err, fmt.Errorf("Expecting HTTP %d, received HTTP %d", HTTPCode, w.Code))
	}

	if w.Body.String() == HTTPBody {
		return err
	}

	opts := jsondiff.DefaultJSONOptions()
	ret, diff := jsondiff.Compare(w.Body.Bytes(), []byte(HTTPBody), &opts)

	// This includes ExactMatch and SupersededMatch (useful for the ping value)
	// but SupersededMatch may accept some errors.
	// TODO: use ExactMatch but doing a manual check for the ping time that will be different.
	if ret == jsondiff.NoMatch {
		err = append(err, fmt.Errorf("%s", diff))
	}

	return err
}

func TestEmptyViewFull(t *testing.T) {

	w := httptest.NewRecorder()
	w.Header().Add("Content-Type", "application/json")

	req, _ := http.NewRequest("GET", "/viewFull", nil)
	ROUTER.ServeHTTP(w, req)

	if errors := assertHTTPAnswerJSON(w, 404, `{"message":"No servers available","success":false}`); errors != nil {
		for _, err := range errors {
			t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
		}
	}
}

func TestEmptyView(t *testing.T) {

	w := httptest.NewRecorder()
	w.Header().Add("Content-Type", "application/json")

	req, _ := http.NewRequest("GET", "/view", nil)
	ROUTER.ServeHTTP(w, req)

	if errors := assertHTTPAnswerJSON(w, 400, `{"message":"you need to submit a platform","success":false}`); errors != nil {
		for _, err := range errors {
			t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
		}
	}
}

func TestInsertServer1(t *testing.T) {
	w := httptest.NewRecorder()
	w.Header().Add("Content-Type", "application/json")

	req, _ := http.NewRequest("POST", "/server", bytes.NewBuffer([]byte(`{
        "game": "Super Chess",
        "server": "http://chess.rogersm.net",
        "serverurl": "http://chess.rogersm.net/server",
        "region": "eu",
        "status": "online",
        "appkey": 1,
        "maxplayers": 2,
        "curplayers": 1,
        "clients": [
            {"platform":"atari16", "url":"http://chess.rogersm.net/atarichess.xex" },
            {"platform": "spectrum2+", "url":"http://chess.rogersm.net/speccychess.xex"}
        ]
    }`)))
	ROUTER.ServeHTTP(w, req)

	if errors := assertHTTPAnswerJSON(w, 201, `{"message":"Server correctly updated","success":true}`); errors != nil {
		for _, err := range errors {
			t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
		}
	}

}

func TestViewFullInsertAndRetrieveServerN(t *testing.T) {

	for _, ServerJson := range GameServersIn {

		w := httptest.NewRecorder()
		w.Header().Add("Content-Type", "application/json")
		req, _ := http.NewRequest("POST", "/server", bytes.NewBuffer([]byte(ServerJson)))
		ROUTER.ServeHTTP(w, req)

		if errors := assertHTTPAnswerJSON(w, 201, `{"message":"Server correctly updated","success":true}`); errors != nil {
			for _, err := range errors {
				t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
			}
		}
	}

	w := httptest.NewRecorder()
	w.Header().Add("Content-Type", "application/json")
	req, _ := http.NewRequest("GET", "/viewFull", nil)
	ROUTER.ServeHTTP(w, req)

	if errors := assertHTTPAnswerJSON(w, 200, GameServersOut); errors != nil {
		for _, err := range errors {
			t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
		}
	}

}

func TestViewInsertAndRetrieveServerN(t *testing.T) {

	for _, ServerJson := range GameServersIn {

		w := httptest.NewRecorder()
		w.Header().Add("Content-Type", "application/json")
		req, _ := http.NewRequest("POST", "/server", bytes.NewBuffer([]byte(ServerJson)))
		ROUTER.ServeHTTP(w, req)

		if errors := assertHTTPAnswerJSON(w, 201, `{"message":"Server correctly updated","success":true}`); errors != nil {
			for _, err := range errors {
				t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
			}
		}
	}

	w := httptest.NewRecorder()
	w.Header().Add("Content-Type", "application/json")
	req, _ := http.NewRequest("GET", "/view", nil)
	ROUTER.ServeHTTP(w, req)

	if errors := assertHTTPAnswerJSON(w, 400, `{"message":"you need to submit a platform","success":false}`); errors != nil {
		for _, err := range errors {
			t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
		}
	}

	w = httptest.NewRecorder()
	w.Header().Add("Content-Type", "application/json")
	req, _ = http.NewRequest("GET", "/view?platform=NoPlatform", nil)
	ROUTER.ServeHTTP(w, req)

	if errors := assertHTTPAnswerJSON(w, 404, `{"message":"No servers available for NoPlatform","success":false}`); errors != nil {
		for _, err := range errors {
			t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
		}
	}

	w = httptest.NewRecorder()
	w.Header().Add("Content-Type", "application/json")
	req, _ = http.NewRequest("GET", "/view?platform=spectrum", nil)
	ROUTER.ServeHTTP(w, req)

	if errors := assertHTTPAnswerJSON(w, 200, GameServersOut); errors != nil {
		for _, err := range errors {
			t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
		}
	}

}

func TestVersion(t *testing.T) {

	w := httptest.NewRecorder()
	w.Header().Add("Content-Type", "application/json")

	req, _ := http.NewRequest("GET", "/version", nil)
	ROUTER.ServeHTTP(w, req)

	if errors := assertHTTPAnswerJSON(w, 200, `{"success": true}`); errors != nil {
		for _, err := range errors {
			t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
		}
	}
}

// A publish used by the production mode tests. The caller swaps in the host it
// wants to exercise.
func privateAddressServer(serverurl string, clienturl string) string {
	return `{
        "game": "Test Game",
        "appkey": 199,
        "server": "lanbox",
        "region": "us",
        "serverurl": "` + serverurl + `",
        "status": "online",
        "maxplayers": 2,
        "curplayers": 0,
        "clients": [
            {"platform":"atari", "url":"` + clienturl + `"}
        ]
    }`
}

// POST /server with an explicit source address. RemoteAddr must be set by hand:
// http.NewRequest leaves it empty, and httptest.NewRequest would default it to
// 192.0.2.1 (TEST-NET-1), which this feature blocks by design.
func postServerFrom(remoteaddr string, body string) *httptest.ResponseRecorder {

	w := httptest.NewRecorder()
	w.Header().Add("Content-Type", "application/json")

	req, _ := http.NewRequest("POST", "/server", bytes.NewBuffer([]byte(body)))
	req.RemoteAddr = remoteaddr

	ROUTER.ServeHTTP(w, req)

	return w
}

func TestProductionRejectsPrivateServerurl(t *testing.T) {

	PRODUCTION = true
	defer func() { PRODUCTION = false }()

	w := postServerFrom("8.8.8.8:1234",
		privateAddressServer("http://192.168.68.61/x", "http://8bitBattleship.com/x.xex"))

	if w.Code != 400 {
		t.Errorf("Expecting HTTP 400, received HTTP %d", w.Code)
	}

	if !strings.Contains(w.Body.String(), "192.168.68.61") {
		t.Errorf("Expecting the rejected host in the response, received %s", w.Body.String())
	}
}

func TestProductionRejectsPrivateClientUrl(t *testing.T) {

	PRODUCTION = true
	defer func() { PRODUCTION = false }()

	w := postServerFrom("8.8.8.8:1234",
		privateAddressServer("http://8bitBattleship.com/x", "http://192.168.68.61/x.xex"))

	if w.Code != 400 {
		t.Errorf("Expecting HTTP 400, received HTTP %d", w.Code)
	}

	if !strings.Contains(w.Body.String(), "192.168.68.61") {
		t.Errorf("Expecting the rejected host in the response, received %s", w.Body.String())
	}
}

func TestProductionRejectsPrivateSource(t *testing.T) {

	PRODUCTION = true
	defer func() { PRODUCTION = false }()

	w := postServerFrom("192.168.68.61:1234",
		privateAddressServer("http://8bitBattleship.com/x", "http://8bitBattleship.com/x.xex"))

	if errors := assertHTTPAnswerJSON(w, 403, `{"success":false,
		"message":"Publishing is not permitted from a non-routable address",
		"errors":["source address 192.168.68.61 is not publicly routable"]}`); errors != nil {
		for _, err := range errors {
			t.Errorf("POST /server %s", err)
		}
	}
}

// a publish that is public end to end must still work in production
func TestProductionAcceptsPublicServer(t *testing.T) {

	PRODUCTION = true
	defer func() { PRODUCTION = false }()

	w := postServerFrom("8.8.8.8:1234",
		privateAddressServer("http://8bitBattleship.com/prodtest", "http://8bitBattleship.com/x.xex"))

	if errors := assertHTTPAnswerJSON(w, 201, `{"message":"Server correctly updated","success":true}`); errors != nil {
		for _, err := range errors {
			t.Errorf("POST /server %s", err)
		}
	}
}

// outside production nothing changes: local addresses stay publishable so that
// developers can keep testing against their own lan.
func TestDevelopmentAcceptsPrivateAddresses(t *testing.T) {

	if PRODUCTION {
		t.Fatal("PRODUCTION leaked from an earlier test")
	}

	w := postServerFrom("192.168.68.61:1234",
		privateAddressServer("http://192.168.68.61/devtest", "http://192.168.68.61/x.xex"))

	if errors := assertHTTPAnswerJSON(w, 201, `{"message":"Server correctly updated","success":true}`); errors != nil {
		for _, err := range errors {
			t.Errorf("POST /server %s", err)
		}
	}
}

func TestViewInsertAndRetrieveServerAppId(t *testing.T) {

	for _, ServerJson := range GameServersIn {

		w := httptest.NewRecorder()
		w.Header().Add("Content-Type", "application/json")
		req, _ := http.NewRequest("POST", "/server", bytes.NewBuffer([]byte(ServerJson)))
		ROUTER.ServeHTTP(w, req)

		if errors := assertHTTPAnswerJSON(w, 201, `{"message":"Server correctly updated","success":true}`); errors != nil {
			for _, err := range errors {
				t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
			}
		}
	}

	w := httptest.NewRecorder()
	w.Header().Add("Content-Type", "application/json")
	req, _ := http.NewRequest("GET", "/view", nil)
	ROUTER.ServeHTTP(w, req)

	if errors := assertHTTPAnswerJSON(w, 400, `{"message":"you need to submit a platform","success":false}`); errors != nil {
		for _, err := range errors {
			t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
		}
	}

	w = httptest.NewRecorder()
	w.Header().Add("Content-Type", "application/json")
	req, _ = http.NewRequest("GET", "/view?platform=NoPlatform&appkey=1", nil)
	ROUTER.ServeHTTP(w, req)

	if errors := assertHTTPAnswerJSON(w, 404, `{"message":"No servers available for NoPlatform","success":false}`); errors != nil {
		for _, err := range errors {
			t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
		}
	}

	w = httptest.NewRecorder()
	w.Header().Add("Content-Type", "application/json")
	req, _ = http.NewRequest("GET", "/view?platform=spectrum&appkey=1", nil)
	ROUTER.ServeHTTP(w, req)

	if errors := assertHTTPAnswerJSON(w, 200, GameServersOutMinAppKey1); errors != nil {
		for _, err := range errors {
			t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
		}
	}

}

func getPath(path string) *httptest.ResponseRecorder {
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", path, nil)
	ROUTER.ServeHTTP(w, req)
	return w
}

func TestGamesSummaryAndList(t *testing.T) {

	for _, ServerJson := range GameServersIn {
		w := httptest.NewRecorder()
		req, _ := http.NewRequest("POST", "/server", bytes.NewBuffer([]byte(ServerJson)))
		ROUTER.ServeHTTP(w, req)
		if w.Code != 201 {
			t.Fatalf("unable to insert test server: HTTP %d", w.Code)
		}
	}

	cases := []struct {
		path string
		code int
		body string
	}{
		{"/games?platform=spectrum", 200, `[{"t":1,"g":"5 CARD STUD","p":1},{"t":2,"g":"Battleship","p":1},{"t":3,"g":"Super Chess","p":1}]`},
		{"/games?platform=spectrum&pagesize=1&offset=1", 200, `[{"t":2,"g":"Battleship","p":1}]`},
		{"/games?platform=spectrum&pagesize=1&page=1", 200, `[{"t":2,"g":"Battleship","p":1}]`},
		{"/games?platform=NoPlatform", 404, `{"message":"No games available","success":false}`},
		{"/games", 400, `{"message":"you need to submit a platform","success":false}`},
		{"/games/2", 400, `{"message":"you need to submit a platform","success":false}`},
		{"/games/0?platform=spectrum", 400, `{"message":"game must be a number from 1 to 255","success":false}`},
		{"/games/battleship?platform=spectrum", 400, `{"message":"game must be a number from 1 to 255","success":false}`},
		{"/games/200?platform=spectrum", 404, `{"message":"No servers available for game 200","success":false}`},
		{"/games/2?platform=spectrum", 200, `[{"i":1,"s":"8bitBattleship.com","m":2,"p":1},{"i":2,"s":"8bitBattleship.com","m":2,"p":0}]`},
		{"/games/2/1?platform=spectrum", 200, `{"u":"https://8bitBattleship.com/battlebots","c":"https://8bitBattleship.com/specship.xex"}`},
		{"/games/2/1", 400, `{"message":"you need to submit a platform","success":false}`},
		{"/games/0/1?platform=spectrum", 400, `{"message":"game must be a number from 1 to 255","success":false}`},
		{"/games/1/9?platform=zx81", 404, `{"message":"No instance available for server 9 on zx81","success":false}`},
		{"/games/2/1?platform=lynx", 404, `{"message":"No instance available for server 1 on lynx","success":false}`},
		{"/games/2/2?platform=spectrum", 200, `{"u":"https://8bitBattleship.com/battlehuman","c":"https://8bitBattleship.com/specship.xex"}`},
		{"/games/2/8bitBattleship.com?platform=spectrum", 400, `{"message":"server must be a server id from /games/2","success":false}`},
	}

	for _, tc := range cases {
		w := getPath(tc.path)
		for _, err := range assertHTTPAnswerJSON(w, tc.code, tc.body) {
			t.Errorf("GET %s %s", tc.path, err)
		}
	}

	// two games sharing an appkey must be counted apart. POST /server now refuses
	// that, but rows from before the check can still hold it, so insert the second
	// one directly.
	body := `{"game":"Chess A","appkey":9,"server":"s","region":"us","serverurl":"http://x/Chess A","status":"online","maxplayers":4,"curplayers":2,"clients":[{"platform":"zx81","url":"http://x/c.xex"}]}`
	req, _ := http.NewRequest("POST", "/server", bytes.NewBuffer([]byte(body)))
	ROUTER.ServeHTTP(httptest.NewRecorder(), req)
	txGameServerUpsert(GameServer{Game: "Chess B", Appkey: 9, Server: "s", Region: "us", Serverurl: "http://x/Chess B",
		Status: "online", Maxplayers: 4, Curplayers: 2, Clients: []GameClient{{Platform: "zx81", Url: "http://x/c.xex"}}}, "")
	for _, err := range assertHTTPAnswerJSON(getPath("/games?platform=zx81"), 200, `[{"t":9,"g":"Chess A","p":2},{"t":9,"g":"Chess B","p":2}]`) {
		t.Errorf("shared appkey: %s", err)
	}

	// bin: 3 byte header, then 19 bytes per game
	w := getPath("/games?platform=spectrum&bin=1")
	if w.Code != 200 || w.Body.Len() != 3+3*19 || w.Body.Bytes()[0] != 3 {
		t.Errorf("unexpected binary summary: HTTP %d, %d bytes", w.Code, w.Body.Len())
	}

	// bin: 3 byte header, then 36 bytes per server
	w = getPath("/games/2?platform=spectrum&bin=1")
	if w.Code != 200 || w.Body.Len() != 3+2*36 || w.Body.Bytes()[0] != 2 {
		t.Errorf("unexpected binary list: HTTP %d, %d bytes", w.Code, w.Body.Len())
	}

	w = getPath("/games/2/1?platform=spectrum&bin=1")
	if w.Code != 200 || w.Body.Len() != 130 {
		t.Errorf("unexpected binary client: HTTP %d, %d bytes", w.Code, w.Body.Len())
	}
}

func TestServerIdsAreLowestFreePerGameType(t *testing.T) {
	mk := func(url string, appkey int) GameServer {
		return GameServer{Game: "Test Game", Appkey: appkey, Server: "s", Region: "us", Serverurl: url,
			Status: "online", Maxplayers: 2, Clients: []GameClient{{Platform: "atari", Url: url + "/c"}}}
	}
	idOf := func(url string) (id int) {
		DATABASE.DB.Get(&id, `SELECT id FROM GameServer WHERE serverurl = $1`, url)
		return id
	}
	urls := []string{"http://id-test-a/s", "http://id-test-b/s", "http://id-test-c/s", "http://id-test-d/s", "http://id-test-other/s"}
	defer func() {
		for _, u := range urls {
			txGameServerDelete(u)
		}
	}()
	// appkey 200 is unlikely to clash with fixtures
	for _, u := range urls[:3] {
		if err := txGameServerUpsert(mk(u, 200), ""); err != nil {
			t.Fatal(err)
		}
	}
	if err := txGameServerUpsert(mk(urls[4], 201), ""); err != nil {
		t.Fatal(err)
	}
	if a, b, c, o := idOf(urls[0]), idOf(urls[1]), idOf(urls[2]), idOf(urls[4]); a != 1 || b != 2 || c != 3 || o != 1 {
		t.Fatalf("ids = %d %d %d (other %d), want 1 2 3 (other 1)", a, b, c, o)
	}

	// re-upsert keeps the id; deleting b frees 2 for the next newcomer
	txGameServerUpsert(mk(urls[2], 200), "")
	txGameServerDelete(urls[1])
	txGameServerUpsert(mk(urls[3], 200), "")
	if c, d := idOf(urls[2]), idOf(urls[3]); c != 3 || d != 2 {
		t.Fatalf("c=%d d=%d, want 3 and 2", c, d)
	}

	// legacy row with no id is numbered by the backfill
	DATABASE.Exec(`UPDATE GameServer SET id = 0 WHERE serverurl = $1`, urls[0])
	if err := txGameServerAssignMissingIds(); err != nil {
		t.Fatal(err)
	}
	if a := idOf(urls[0]); a != 1 {
		t.Fatalf("backfilled id = %d, want 1", a)
	}
}

// an appkey belongs to one game: another name under it is refused, the same name is not
func TestAppkeyOfAnotherGameIsRejected(t *testing.T) {
	post := func(game, serverurl string) *httptest.ResponseRecorder {
		w := httptest.NewRecorder()
		req, _ := http.NewRequest("POST", "/server", bytes.NewBuffer([]byte(`{"game":"`+game+`","appkey":210,"server":"s","region":"us","serverurl":"`+serverurl+`","status":"online","maxplayers":2,"curplayers":0,"clients":[{"platform":"atari","url":"http://y/c.xex"}]}`)))
		ROUTER.ServeHTTP(w, req)
		return w
	}
	urls := []string{"http://y/a", "http://y/b", "http://y/c"}
	defer func() {
		for _, u := range urls {
			txGameServerDelete(u)
		}
	}()

	if w := post("Reversi", urls[0]); w.Code != 201 {
		t.Fatalf("first server: HTTP %d %s", w.Code, w.Body.String())
	}

	// a server alone under its appkey may rename its game
	if w := post("Reversi II", urls[0]); w.Code != 201 {
		t.Fatalf("rename: HTTP %d %s", w.Code, w.Body.String())
	}

	if w := post("Reversi II", urls[1]); w.Code != 201 {
		t.Fatalf("same game: HTTP %d %s", w.Code, w.Body.String())
	}

	// the game list groups by exact name, so a different case is another game
	if w := post("REVERSI ii", urls[2]); w.Code != 400 {
		t.Fatalf("same game, other case: HTTP %d %s", w.Code, w.Body.String())
	}

	w := post("Othello", urls[2])
	if errors := assertHTTPAnswerJSON(w, 400, `{"success":false,
		"message":"VALIDATEERR - Appkey belongs to another game",
		"errors":["appkey 210 is registered to game 'Reversi II', not 'Othello'. Use exactly 'Reversi II' (case included) as the game name, or register a new appkey for your game at `+APPKEY_REGISTRY_URL+`"]}`); errors != nil {
		for _, err := range errors {
			t.Errorf("POST /server %s", err)
		}
	}
}
