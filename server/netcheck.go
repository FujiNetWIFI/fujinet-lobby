package main

import (
	"net/netip"
	"net/url"
	"strings"
)

// ranges that are not globally routable but that net/netip has no predicate for.
var unroutablePrefixes = []netip.Prefix{
	netip.MustParsePrefix("100.64.0.0/10"),   // RFC 6598 CGNAT (also Tailscale)
	netip.MustParsePrefix("192.0.0.0/24"),    // RFC 6890 IETF protocol assignments
	netip.MustParsePrefix("192.0.2.0/24"),    // TEST-NET-1
	netip.MustParsePrefix("198.51.100.0/24"), // TEST-NET-2
	netip.MustParsePrefix("203.0.113.0/24"),  // TEST-NET-3
	netip.MustParsePrefix("198.18.0.0/15"),   // RFC 2544 benchmarking
	netip.MustParsePrefix("240.0.0.0/4"),     // reserved, includes 255.255.255.255
	netip.MustParsePrefix("64:ff9b::/96"),    // IPv4/IPv6 translation
	netip.MustParsePrefix("2001:db8::/32"),   // documentation
	netip.MustParsePrefix("2001:20::/28"),    // ORCHIDv2
	netip.MustParsePrefix("100::/64"),        // discard-only
}

// hostnames that can only ever resolve inside a local network.
var unroutableSuffixes = []string{".local", ".internal", ".lan", ".home.arpa"}

// true if the address cannot be reached from the public internet
func IsUnroutableAddr(addr netip.Addr) bool {

	if !addr.IsValid() {
		return false
	}

	// so ::ffff:192.168.68.61 is judged as the 192.168.68.61 it really is
	addr = addr.Unmap()

	if addr.IsLoopback() ||
		addr.IsPrivate() ||
		addr.IsUnspecified() ||
		addr.IsLinkLocalUnicast() ||
		addr.IsLinkLocalMulticast() ||
		addr.IsInterfaceLocalMulticast() ||
		addr.IsMulticast() {
		return true
	}

	for _, prefix := range unroutablePrefixes {
		if prefix.Contains(addr) {
			return true
		}
	}

	return false
}

// true if the host (an ip literal or a name, as returned by url.URL.Hostname())
// cannot be reached from the public internet.
//
// Names are never resolved: a DNS lookup would block the request handler and the
// answer can change after we accept it anyway, so we only reject the names that
// are local-only by definition.
func IsUnroutableHost(host string) bool {

	if len(host) == 0 {
		return false
	}

	if addr, err := netip.ParseAddr(host); err == nil {
		return IsUnroutableAddr(addr)
	}

	host = strings.ToLower(strings.TrimSuffix(host, "."))

	if host == "localhost" {
		return true
	}

	for _, suffix := range unroutableSuffixes {
		if strings.HasSuffix(host, suffix) {
			return true
		}
	}

	return false
}

// pull the host out of a url and report whether it is unroutable. A url we cannot
// parse is not our problem here: CheckInput already validates url syntax and we
// don't want to report the same defect twice.
func UnroutableURLHost(rawurl string) (host string, unroutable bool) {

	parsed, err := url.Parse(rawurl)

	if err != nil {
		return "", false
	}

	host = parsed.Hostname()

	return host, IsUnroutableHost(host)
}
