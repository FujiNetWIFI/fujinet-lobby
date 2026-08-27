package main

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

// sampleMin is a filled-in record, so the tests below catch a field being
// written at the wrong offset rather than only a wrong total length.
func sampleMin() GameServerMin {
	return GameServerMin{
		Game:       "Super Chess",
		AppKey:     7,
		Serverurl:  "http://chess.example.com/server",
		Client:     "tnfs://chess.example.com/chess.xex",
		Server:     "EU Table 1",
		Region:     "eu",
		Online:     1,
		Maxplayers: 4,
		Curplayers: 2,
		ChatUrl:    "HTTP://Q.TNFS.IO/6EWL3G",
	}
}

// TestBinFormatV1StrideUnchanged is the regression guard that matters most in
// this change. Deployed 8-bit clients read these records as a fixed-stride
// struct straight into memory, so a stride change does not fail for them -- it
// silently shifts every subsequent record. Format 1 must stay 189 bytes.
func TestBinFormatV1StrideUnchanged(t *testing.T) {
	got := sampleMin().appendAsBinary(nil, BinFormatV1)

	if len(got) != BinRecordLenV1 {
		t.Fatalf("format 1 record is %d bytes, want %d", len(got), BinRecordLenV1)
	}
}

// TestBinFormatV1IgnoresChatUrl proves a chat URL cannot leak into the old
// format even when one is set.
func TestBinFormatV1IgnoresChatUrl(t *testing.T) {
	with := sampleMin()
	without := sampleMin()
	without.ChatUrl = ""

	a := with.appendAsBinary(nil, BinFormatV1)
	b := without.appendAsBinary(nil, BinFormatV1)

	if !bytes.Equal(a, b) {
		t.Error("format 1 output changed when a chat url was set")
	}
}

func TestBinFormatV2AppendsChatUrl(t *testing.T) {
	rec := sampleMin()
	got := rec.appendAsBinary(nil, BinFormatV2)

	if len(got) != BinRecordLenV2 {
		t.Fatalf("format 2 record is %d bytes, want %d", len(got), BinRecordLenV2)
	}

	// Format 2 must be format 1 with the field appended, not a reshuffle.
	if !bytes.Equal(got[:BinRecordLenV1], rec.appendAsBinary(nil, BinFormatV1)) {
		t.Error("the first 189 bytes of format 2 differ from format 1")
	}

	field := got[BinRecordLenV1:]
	if len(field) != chatUrlFieldLen {
		t.Fatalf("chat url field is %d bytes, want %d", len(field), chatUrlFieldLen)
	}

	nul := bytes.IndexByte(field, 0)
	if nul < 0 {
		t.Fatal("chat url field is not NUL terminated; a C client would run off the end")
	}
	if got := string(field[:nul]); got != rec.ChatUrl {
		t.Errorf("chat url = %q, want %q", got, rec.ChatUrl)
	}
	for i, b := range field[nul:] {
		if b != 0 {
			t.Errorf("byte %d after the terminator is %#x, want zero padding", nul+i, b)
		}
	}
}

// TestBinFormatV2TruncatesOverlongChatUrl checks the field cannot overflow its
// slot even if something upstream lets a long URL through.
func TestBinFormatV2TruncatesOverlongChatUrl(t *testing.T) {
	rec := sampleMin()
	rec.ChatUrl = strings.Repeat("X", 100)

	got := rec.appendAsBinary(nil, BinFormatV2)
	if len(got) != BinRecordLenV2 {
		t.Fatalf("record is %d bytes, want %d", len(got), BinRecordLenV2)
	}
	if got[len(got)-1] != 0 {
		t.Error("a truncated chat url left the field without a terminator")
	}
}

func TestParseBinFormat(t *testing.T) {
	tests := map[string]int{
		"":       0,
		"0":      0,
		"1":      BinFormatV1,
		"2":      BinFormatV2,
		"3":      0, // a future format this build does not know
		"true":   0,
		"1;DROP": 0,
	}
	for in, want := range tests {
		if got := parseBinFormat(in); got != want {
			t.Errorf("parseBinFormat(%q) = %d, want %d", in, got, want)
		}
	}
}

// --- chat service client -------------------------------------------------

// withChatService points the client at a stub and restores the globals after.
func withChatService(t *testing.T, handler http.HandlerFunc) {
	t.Helper()

	srv := httptest.NewServer(handler)
	oldURL, oldKey := CHATSRV_URL, CHATSRV_APIKEY
	CHATSRV_URL, CHATSRV_APIKEY = srv.URL, "test-key"

	t.Cleanup(func() {
		srv.Close()
		CHATSRV_URL, CHATSRV_APIKEY = oldURL, oldKey
	})
}

func TestChatRoomUpsertDisabled(t *testing.T) {
	oldURL, oldKey := CHATSRV_URL, CHATSRV_APIKEY
	CHATSRV_URL, CHATSRV_APIKEY = "", ""
	defer func() { CHATSRV_URL, CHATSRV_APIKEY = oldURL, oldKey }()

	if got := ChatRoomUpsert(GameServer{Serverurl: "http://a/s"}); got != "" {
		t.Errorf("ChatRoomUpsert with no chat service = %q, want empty", got)
	}
}

func TestChatRoomUpsertHappyPath(t *testing.T) {
	var gotKey, gotMethod, gotPath string
	var gotBody chatRoomRequest

	withChatService(t, func(w http.ResponseWriter, r *http.Request) {
		gotKey = r.Header.Get("X-Api-Key")
		gotMethod = r.Method
		gotPath = r.URL.Path
		json.NewDecoder(r.Body).Decode(&gotBody)
		w.Header().Set("Content-Type", "application/json")
		w.Write([]byte(`{"code":"6EWL3G","url":"HTTP://Q.TNFS.IO/6EWL3G"}`))
	})

	got := ChatRoomUpsert(GameServer{
		Serverurl: "http://chess.example.com/server", Game: "Super Chess",
		Server: "EU Table 1", Maxplayers: 4,
	})

	if got != "HTTP://Q.TNFS.IO/6EWL3G" {
		t.Errorf("url = %q", got)
	}
	if gotMethod != "POST" || gotPath != "/api/rooms" {
		t.Errorf("called %s %s, want POST /api/rooms", gotMethod, gotPath)
	}
	if gotKey != "test-key" {
		t.Errorf("X-Api-Key = %q, want the configured key", gotKey)
	}
	if gotBody.Serverurl != "http://chess.example.com/server" || gotBody.Maxplayers != 4 {
		t.Errorf("request body = %+v", gotBody)
	}
}

// TestChatRoomUpsertFailuresAreSilent covers the rule that a chat problem must
// never stop a game server registering.
func TestChatRoomUpsertFailuresAreSilent(t *testing.T) {
	cases := map[string]http.HandlerFunc{
		"500": func(w http.ResponseWriter, r *http.Request) { w.WriteHeader(500) },
		"401": func(w http.ResponseWriter, r *http.Request) { w.WriteHeader(401) },
		"garbage": func(w http.ResponseWriter, r *http.Request) {
			w.Write([]byte("not json"))
		},
		"empty body": func(w http.ResponseWriter, r *http.Request) {},
	}

	for name, handler := range cases {
		t.Run(name, func(t *testing.T) {
			withChatService(t, handler)
			if got := ChatRoomUpsert(GameServer{Serverurl: "http://a/s"}); got != "" {
				t.Errorf("got %q, want empty on a %s response", got, name)
			}
		})
	}
}

// TestChatRoomUpsertRejectsOverlongUrl guards the wire format: the field is a
// fixed 26-byte slot, and silently truncating a URL would produce a QR code
// that scans to the wrong address, which is worse than no QR at all.
func TestChatRoomUpsertRejectsOverlongUrl(t *testing.T) {
	withChatService(t, func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte(`{"code":"X","url":"HTTP://SOMETHING.MUCH.LONGER.EXAMPLE/ABCDEF"}`))
	})

	if got := ChatRoomUpsert(GameServer{Serverurl: "http://a/s"}); got != "" {
		t.Errorf("got %q, want empty: the url does not fit the wire format", got)
	}
}

func TestChatRoomUpsertAcceptsMaximumLengthUrl(t *testing.T) {
	max := "HTTP://Q.TNFS.IO/ABCDEFGH" // 25 characters, the QR v1 limit
	withChatService(t, func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte(`{"code":"ABCDEFGH","url":"` + max + `"}`))
	})

	if got := ChatRoomUpsert(GameServer{Serverurl: "http://a/s"}); got != max {
		t.Errorf("got %q, want %q accepted at exactly the field width", got, max)
	}
}

// --- end to end through the handlers -------------------------------------

func postServer(t *testing.T, body string) *httptest.ResponseRecorder {
	t.Helper()
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/server", bytes.NewBuffer([]byte(body)))
	ROUTER.ServeHTTP(w, req)
	return w
}

const chatTestServer = `{
	"game": "Chat Test",
	"appkey": 9,
	"server": "chat.example.com",
	"serverurl": "http://chat.example.com/server",
	"region": "eu",
	"status": "online",
	"maxplayers": 2,
	"curplayers": 1,
	"clients": [{"platform":"atari", "url":"http://chat.example.com/c.xex"}]
}`

func storedChatUrl(t *testing.T, serverurl string) string {
	t.Helper()
	var got string
	if err := DATABASE.DB.Get(&got, `SELECT chat_url FROM GameServer WHERE Serverurl = ?`, serverurl); err != nil {
		t.Fatalf("reading chat_url: %v", err)
	}
	return got
}

// TestChatUrlIsAssignedNotAccepted checks that a game server cannot set its own
// chat URL. Publishers are untrusted -- anyone can POST to /server -- so a
// self-declared URL would let them put an arbitrary link in front of players.
func TestChatUrlIsAssignedNotAccepted(t *testing.T) {
	withChatService(t, func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte(`{"code":"6EWL3G","url":"HTTP://Q.TNFS.IO/6EWL3G"}`))
	})

	body := strings.Replace(chatTestServer, `"region": "eu",`,
		`"region": "eu", "chaturl": "HTTP://EVIL.EXAMPLE/XX",`, 1)

	if w := postServer(t, body); w.Code != http.StatusCreated {
		t.Fatalf("POST /server = %d: %s", w.Code, w.Body.String())
	}
	defer DATABASE.Exec(`DELETE FROM GameServer WHERE Serverurl = ?`, "http://chat.example.com/server")

	if got := storedChatUrl(t, "http://chat.example.com/server"); got != "HTTP://Q.TNFS.IO/6EWL3G" {
		t.Errorf("stored chat_url = %q, want the service-assigned url, not the publisher's", got)
	}
}

// TestChatUrlSurvivesRepingDuringOutage is the reason txGameServerUpsert reads
// the existing value back: the upsert is a DELETE followed by an INSERT, so
// without carrying it forward a chat outage would blank the URL of every live
// server the moment it next pinged.
func TestChatUrlSurvivesRepingDuringOutage(t *testing.T) {
	withChatService(t, func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte(`{"code":"6EWL3G","url":"HTTP://Q.TNFS.IO/6EWL3G"}`))
	})

	if w := postServer(t, chatTestServer); w.Code != http.StatusCreated {
		t.Fatalf("first POST = %d: %s", w.Code, w.Body.String())
	}
	defer DATABASE.Exec(`DELETE FROM GameServer WHERE Serverurl = ?`, "http://chat.example.com/server")

	if got := storedChatUrl(t, "http://chat.example.com/server"); got == "" {
		t.Fatal("first registration stored no chat url")
	}

	// Chat service now down: the re-ping must keep the URL we already had.
	withChatService(t, func(w http.ResponseWriter, r *http.Request) { w.WriteHeader(503) })

	if w := postServer(t, chatTestServer); w.Code != http.StatusCreated {
		t.Fatalf("re-ping = %d: %s", w.Code, w.Body.String())
	}

	if got := storedChatUrl(t, "http://chat.example.com/server"); got != "HTTP://Q.TNFS.IO/6EWL3G" {
		t.Errorf("chat_url after a re-ping during an outage = %q, want it preserved", got)
	}
}

// TestViewBinaryStrides walks the real endpoint and checks both formats have a
// whole number of records of the expected size.
func TestViewBinaryStrides(t *testing.T) {
	withChatService(t, func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte(`{"code":"6EWL3G","url":"HTTP://Q.TNFS.IO/6EWL3G"}`))
	})

	if w := postServer(t, chatTestServer); w.Code != http.StatusCreated {
		t.Fatalf("POST /server = %d: %s", w.Code, w.Body.String())
	}
	defer DATABASE.Exec(`DELETE FROM GameServer WHERE Serverurl = ?`, "http://chat.example.com/server")

	for _, tc := range []struct{ bin, stride int }{
		{BinFormatV1, BinRecordLenV1},
		{BinFormatV2, BinRecordLenV2},
	} {
		w := httptest.NewRecorder()
		req, _ := http.NewRequest("GET",
			"/view?platform=atari&appkey=9&bin="+string(rune('0'+tc.bin)), nil)
		ROUTER.ServeHTTP(w, req)

		if w.Code != http.StatusOK {
			t.Fatalf("bin=%d: GET /view = %d: %s", tc.bin, w.Code, w.Body.String())
		}

		body := w.Body.Bytes()
		const header = 3
		if len(body) < header {
			t.Fatalf("bin=%d: response is %d bytes, shorter than the header", tc.bin, len(body))
		}
		count := int(body[0])
		if want := header + count*tc.stride; len(body) != want {
			t.Errorf("bin=%d: %d bytes for %d records, want %d (stride %d)",
				tc.bin, len(body), count, want, tc.stride)
		}

		// The chat url must be present in format 2 and absent from format 1.
		hasChat := bytes.Contains(body, []byte("HTTP://Q.TNFS.IO/6EWL3G"))
		if tc.bin == BinFormatV2 && !hasChat {
			t.Error("bin=2 response does not carry the chat url")
		}
		if tc.bin == BinFormatV1 && hasChat {
			t.Error("bin=1 response leaked the chat url")
		}
	}
}
