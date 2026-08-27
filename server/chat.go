package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"
)

// Client for the FujiNet game chat service (fujinet-game-system-chat).
//
// Each lobby room gets a companion web room there for text chat and WebRTC
// voice/video. The chat service assigns a short URL, which the lobby stores and
// hands to 8-bit clients so they can draw it as a QR code for players to scan.
//
// The URL has to fit a QR version 1 symbol -- 21x21 modules, the largest an
// Intellivision can render -- so it is at most 25 characters. That is the chat
// service's constraint to enforce; the lobby just carries the string.

var (
	CHATSRV_URL    string // base url of the chat service, empty disables the feature
	CHATSRV_APIKEY string
)

// chatTimeout bounds how long a game server registration can be delayed by the
// chat service. Matches the event webhook timeout.
const chatTimeout = 2 * time.Second

type chatRoomRequest struct {
	Serverurl  string `json:"serverurl"`
	Game       string `json:"game"`
	Server     string `json:"server"`
	Maxplayers int    `json:"maxplayers"`
}

type chatRoomResponse struct {
	Code string `json:"code"`
	Url  string `json:"url"`
}

// ChatEnabled reports whether a chat service was configured at startup.
func ChatEnabled() bool {
	return CHATSRV_URL != "" && CHATSRV_APIKEY != ""
}

// ChatRoomUpsert asks the chat service for this server's room URL, creating the
// room if it does not exist yet and refreshing its lifetime if it does.
//
// The call is idempotent on serverurl: a game server re-pings the lobby
// constantly, and a URL that changed on each ping would invalidate a QR code a
// player scanned moments earlier.
//
// Returns an empty string on any failure. Callers must treat that as "no chat
// for this server" and carry on -- a chat outage must never stop a game server
// registering.
func ChatRoomUpsert(gs GameServer) string {

	if !ChatEnabled() {
		return ""
	}

	body, err := json.Marshal(chatRoomRequest{
		Serverurl:  gs.Serverurl,
		Game:       gs.Game,
		Server:     gs.Server,
		Maxplayers: gs.Maxplayers,
	})
	if err != nil {
		ERROR.Printf("Unable to marshal chat room request for %s (%s)", gs.Serverurl, err)
		return ""
	}

	resp, err := chatRequest("POST", body)
	if err != nil {
		ERROR.Printf("Unable to register chat room for %s (%s)", gs.Serverurl, err)
		return ""
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		ERROR.Printf("Chat service returned %s registering a room for %s",
			resp.Status, gs.Serverurl)
		return ""
	}

	var room chatRoomResponse
	if err := json.NewDecoder(io.LimitReader(resp.Body, 4096)).Decode(&room); err != nil {
		ERROR.Printf("Unable to decode chat room response for %s (%s)", gs.Serverurl, err)
		return ""
	}

	// The field is a fixed-width 26-byte slot in the binary record. A longer
	// string would be silently truncated into an unscannable URL, so drop it
	// and let the client skip the QR screen instead.
	if len(room.Url) > chatUrlFieldLen-1 {
		ERROR.Printf("Chat service returned a %d character url for %s, over the %d the wire format allows: %s",
			len(room.Url), gs.Serverurl, chatUrlFieldLen-1, room.Url)
		return ""
	}

	DEBUG.Printf("Chat room for %s is %s", gs.Serverurl, room.Url)

	return room.Url
}

// ChatRoomDelete releases the chat room for a deregistered game server. Errors
// are logged and swallowed: the chat service reaps unping'd rooms on its own,
// so a failure here costs a code until it expires, nothing more.
func ChatRoomDelete(serverurl string) {

	if !ChatEnabled() {
		return
	}

	body, err := json.Marshal(chatRoomRequest{Serverurl: serverurl})
	if err != nil {
		ERROR.Printf("Unable to marshal chat room delete for %s (%s)", serverurl, err)
		return
	}

	resp, err := chatRequest("DELETE", body)
	if err != nil {
		ERROR.Printf("Unable to delete chat room for %s (%s)", serverurl, err)
		return
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusNoContent && resp.StatusCode != http.StatusOK {
		ERROR.Printf("Chat service returned %s deleting the room for %s", resp.Status, serverurl)
	}
}

func chatRequest(method string, body []byte) (*http.Response, error) {

	req, err := http.NewRequest(method, CHATSRV_URL+"/api/rooms", bytes.NewBuffer(body))
	if err != nil {
		return nil, fmt.Errorf("building request: %w", err)
	}

	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-Api-Key", CHATSRV_APIKEY)
	req.Header.Set("X-Lobby-Client", VERSION)

	client := &http.Client{Timeout: chatTimeout}

	return client.Do(req)
}
