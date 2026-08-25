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

	os.Exit(m.Run())
}

var GameServersIn = []string{
	`{
        "game": "Super Chess",
        "appkey": 1,
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
        "appkey": 2,
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
        "appkey": 3,
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
        "appkey": 3,
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
        "appkey": 3,
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
        "appkey": 2,
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
        "appkey": 3,
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
        "appkey": 3,
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

var GameServersOutMin = `[{"g":"Battleship","t":3,"u":"https://8bitBattleship.com/battlehuman","c":"https://8bitBattleship.com/specship.xex","s":"8bitBattleship.com","r":"apac","o":1,"m":2,"p":0,"a":0},{"g":"5 CARD STUD","t":2,"u":"tcp://thomcorner.com/pokerbots","c":"tcp://thomcorner.com/clientus/specpoker.xex","s":"erichomeserver.com","r":"us","o":1,"m":8,"p":1,"a":0},{"g":"Battleship","t":2,"u":"https://8bitBattleship.com/battlebots","c":"https://8bitBattleship.com/specship.xex","s":"8bitBattleship.com","r":"au","o":1,"m":2,"p":1,"a":0},{"g":"Super Chess","t":1,"u":"http://chess.rogersm.net/server","c":"http://chess.rogersm.net/speccychess.xex","s":"chess.rogersm.net","r":"eu","o":1,"m":2,"p":1,"a":0},{"g":"5 CARD STUD","t":3,"u":"tcp://thomcorner.com/server5","c":"tcp://thomcorner.com/specpoker.xex","s":"erichomeserver.com","r":"all","o":0,"m":3,"p":0,"a":0}]`
var GameServersOutMinAppKey2 = `[{"g":"Battleship","t":2,"u":"https://8bitBattleship.com/battlebots","c":"https://8bitBattleship.com/specship.xex","s":"8bitBattleship.com","r":"au","o":1,"m":2,"p":1,"a":0},{"g":"5 CARD STUD","t":2,"u":"tcp://thomcorner.com/pokerbots","c":"tcp://thomcorner.com/clientus/specpoker.xex","s":"erichomeserver.com","r":"us","o":1,"m":8,"p":1,"a":0}]`

func setupRouter() *gin.Engine {

	router := gin.Default()

	router.GET("/viewFull", ShowServers)
	router.GET("/view", ShowServersMinimised)
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
        "appkey": 1,
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
	req, _ = http.NewRequest("GET", "/view?platform=spectrum&appkey=2", nil)
	ROUTER.ServeHTTP(w, req)

	if errors := assertHTTPAnswerJSON(w, 200, GameServersOutMinAppKey2); errors != nil {
		for _, err := range errors {
			t.Errorf("%s %s %s", req.Method, req.URL.Path, err)
		}
	}

}
