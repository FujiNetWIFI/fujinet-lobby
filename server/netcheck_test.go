package main

import "testing"

func TestIsUnroutableHost(t *testing.T) {

	unroutable := []string{
		"192.168.68.61", // the address that prompted this check
		"192.168.0.1",
		"10.0.0.5",
		"172.16.3.1",
		"172.31.255.254",
		"127.0.0.1",
		"169.254.10.1", // link local
		"100.64.0.1",   // CGNAT
		"192.0.2.5",    // TEST-NET-1
		"198.51.100.5", // TEST-NET-2
		"203.0.113.5",  // TEST-NET-3
		"198.18.0.1",   // benchmarking
		"224.0.0.1",    // multicast
		"255.255.255.255",
		"0.0.0.0",
		"::1",
		"::",
		"fe80::1",
		"fd00::1",
		"2001:db8::1",
		"::ffff:192.168.68.61", // ipv4 mapped, must not slip past
		"localhost",
		"LOCALHOST",
		"myserver.local",
		"fujinet.internal",
		"nas.lan",
		"printer.home.arpa",
		"myserver.local.", // fully qualified with the trailing root dot
	}

	for _, host := range unroutable {
		if !IsUnroutableHost(host) {
			t.Errorf("IsUnroutableHost(%q) = false, want true", host)
		}
	}

	routable := []string{
		"8.8.8.8",
		"1.1.1.1",
		"172.32.0.1", // just outside 172.16/12
		"172.15.0.1", // just below 172.16/12
		"100.128.0.1",
		"9.255.255.255",
		"11.0.0.1",
		"2606:4700::1111",
		"chess.rogersm.net",
		"8bitBattleship.com",
		"thomcorner.com",
		"5card.carr-designs.com",
		"notlocalhost.com",
		"", // no host at all is somebody else's validation problem
	}

	for _, host := range routable {
		if IsUnroutableHost(host) {
			t.Errorf("IsUnroutableHost(%q) = true, want false", host)
		}
	}
}

func TestUnroutableURLHost(t *testing.T) {

	tests := []struct {
		rawurl     string
		host       string
		unroutable bool
	}{
		{"http://192.168.68.61:8080/poker", "192.168.68.61", true},
		{"http://192.168.68.61/x.xex", "192.168.68.61", true},
		{"tcp://10.0.0.5/pokerbots", "10.0.0.5", true},
		{"http://[::1]:8080/x", "::1", true}, // brackets stripped by Hostname()
		{"http://localhost:8080/server", "localhost", true},
		{"https://chess.rogersm.net/server", "chess.rogersm.net", false},
		{"tcp://thomcorner.com/clientus/ataripoker.xex", "thomcorner.com", false},
		{"https://5card.carr-designs.com/?table=NORM&count=2", "5card.carr-designs.com", false},
		{"http://8.8.8.8:1234/x", "8.8.8.8", false},
		// CheckInput owns url syntax, we must not report the same defect twice
		{"not a url", "", false},
		{"/relative/path", "", false},
		{"", "", false},
	}

	for _, test := range tests {

		host, unroutable := UnroutableURLHost(test.rawurl)

		if host != test.host || unroutable != test.unroutable {
			t.Errorf("UnroutableURLHost(%q) = (%q, %t), want (%q, %t)",
				test.rawurl, host, unroutable, test.host, test.unroutable)
		}
	}
}

func TestCheckRoutableAddresses(t *testing.T) {

	public := GameServer{
		Serverurl: "https://8bitBattleship.com/battlebots",
		Clients: []GameClient{
			{Platform: "atari", Url: "https://8bitBattleship.com/atariship.xex"},
		},
	}

	if err := public.CheckRoutableAddresses(); err != nil {
		t.Errorf("a fully public server was rejected: %s", err)
	}

	privateServerurl := public
	privateServerurl.Serverurl = "http://192.168.68.61/battlebots"

	if err := privateServerurl.CheckRoutableAddresses(); err == nil {
		t.Error("a private serverurl was accepted, want rejected")
	}

	privateClient := public
	privateClient.Clients = []GameClient{
		{Platform: "atari", Url: "http://192.168.68.61/atariship.xex"},
	}

	if err := privateClient.CheckRoutableAddresses(); err == nil {
		t.Error("a private client url was accepted, want rejected")
	}
}
