package main

import (
	"bytes"
	_ "embed"
	"flag"
	"fmt"
	"log"
	"net"
	"net/url"
	"os"
	"os/signal"
	"runtime"
	"strings"
	"syscall"
	"time"

	"github.com/gin-gonic/gin"

	"github.com/madflojo/tasks"
)

var (
	WARN   CustomLogger
	INFO   CustomLogger
	ERROR  CustomLogger
	DEBUG  CustomLogger
	LOGGER CustomLogger
	DB     CustomLogger
)

var (
	DATABASE           *lobbyDB
	SCHEDULER          *tasks.Scheduler
	TIME               uint64
	STARTEDON          time.Time
	EVTSERVER_WEBHOOKS []string
	PRODUCTION         bool
)

const (
	VERSION   = "5.5.7"
	STRINGVER = "fujinet persistent lobby  " + VERSION + "/" + runtime.GOOS + " (c) Roger Sen 2025"
)

//go:embed doc.html
var DOCHTML []byte

//go:embed servers.html
var SERVERS_HTML []byte

func main() {

	var srvaddr, chatsrv, chatkey string
	var evtaddrs, trustedproxies ArrayOfParams
	var help, version bool

	flag.StringVar(&srvaddr, "srvaddr", ":8080", "<address:port> for http server")
	flag.StringVar(&chatsrv, "chatsrv", "", "<url> of the game chat service, e.g. https://q.tnfs.io (empty disables chat rooms)")
	flag.StringVar(&chatkey, "chatkey", os.Getenv("LOBBY_CHATKEY"), "shared secret for the chat service api (or set LOBBY_CHATKEY)")
	flag.Var(&evtaddrs, "evtaddr", "<http> for event server webhook (multiple values accepted)")
	flag.Var(&trustedproxies, "trustedproxy", "<ip|cidr> of a reverse proxy allowed to set X-Forwarded-For (multiple values accepted)")

	flag.BoolVar(&PRODUCTION, "prod", false, "reject publishes coming from, or advertising, a non-routable address")
	flag.BoolVar(&version, "version", false, "show current version")
	flag.BoolVar(&help, "help", false, "show this help")

	flag.Parse()

	if version {
		fmt.Fprintln(os.Stderr, VERSION)
		return
	}

	if help || len(srvaddr) == 0 {
		flag.PrintDefaults()
		return
	}

	init_logger()
	init_os_signal()
	init_scheduler()
	init_time()
	init_db()
	init_html(srvaddr)
	init_webhook(evtaddrs)
	init_chat(chatsrv, chatkey)

	router := gin.Default()

	init_trusted_proxies(router, trustedproxies)

	router.GET("/", ShowServersHtml)
	router.GET("/docs", ShowDocs)
	router.GET("/viewFull", ShowServers)
	router.GET("/view", ShowServersMinimised)
	router.GET("/version", ShowStatus)
	router.POST("/server", UpsertServer)
	router.DELETE("/server", DeleteServer)

	router.Run(srvaddr)

}

/*
 * Subsystems start here.
 */

// init_chat configures the link to the game chat service. Chat rooms are opt-in
// per deployment: with no -chatsrv the lobby behaves exactly as before and the
// chat_url field goes out empty.
func init_chat(chatsrv string, chatkey string) {

	if chatsrv == "" {
		INFO.Println("No chat service configured, chat room urls will be empty")
		return
	}

	parsed, err := url.Parse(chatsrv)
	if err != nil || parsed.Host == "" {
		WARN.Printf("%s is not a valid url for the chat service. Chat rooms are disabled", chatsrv)
		return
	}

	if chatkey == "" {
		WARN.Printf("-chatsrv is set but no -chatkey was given; the chat api rejects unauthenticated calls. Chat rooms are disabled")
		return
	}

	CHATSRV_URL = strings.TrimRight(chatsrv, "/")
	CHATSRV_APIKEY = chatkey

	INFO.Printf("%s will be used as the game chat service", CHATSRV_URL)
}

func init_logger() {

	INFO = NewCustomLogger("info", "INFO: ", log.LstdFlags)
	WARN = NewCustomLogger("warn", "WARN: ", log.LstdFlags)
	ERROR = NewCustomLogger("error", "ERROR: ", log.LstdFlags)
	LOGGER = NewCustomLogger("logger", "LOGGER: ", log.LstdFlags)
	DEBUG = NewCustomLogger("debug", "DEBUG: ", log.LstdFlags|log.Lshortfile)
	DB = NewCustomLogger("db", "DB: ", log.LstdFlags)

	value, ok := os.LookupEnv("LOG_LEVEL")

	if ok && value == "PROD" {
		DEBUG.SetActive(false)

		// -prod already having set this true is fine, the flag can only turn it on
		PRODUCTION = true
	}

	if PRODUCTION {
		INFO.Println("running in production mode: publishes from, or advertising, a non-routable address will be rejected")
	} else {
		INFO.Println("running in development mode: non-routable addresses are accepted")
	}
}

// tell gin which hops may set X-Forwarded-For. Without this gin trusts the header
// from any source, so the source address check below could be bypassed by simply
// sending a forged one.
func init_trusted_proxies(router *gin.Engine, trustedproxies ArrayOfParams) {

	if len(trustedproxies) == 0 {
		if PRODUCTION {
			WARN.Println("no -trustedproxy given: X-Forwarded-For is trusted from any source and the client address cannot be relied upon")
		}

		return
	}

	if err := router.SetTrustedProxies(trustedproxies); err != nil {
		ERROR.Printf("unable to set trusted proxies %v (%s). X-Forwarded-For will be trusted from any source.", trustedproxies, err)
		return
	}

	INFO.Printf("trusting X-Forwarded-For from %v", trustedproxies)
}

func init_scheduler() error {
	SCHEDULER := tasks.New()

	TIME = 0

	SCHEDULER.Add(&tasks.Task{
		Interval: time.Duration(1 * time.Second),
		TaskFunc: ticker("a 1 sec ticker"),
	})

	return nil

}

// TODO, we should be able to add parameters to the function to exec w/o closures
func ticker(s string) func() error {

	return func() error {

		TIME += 1

		return nil
	}
}

func init_os_signal() {

	sigchnl := make(chan os.Signal, 1)
	signal.Notify(sigchnl)

	go SignalHandler(sigchnl)
}

func SignalHandler(sigchan chan os.Signal) {

	for {
		signal := <-sigchan

		switch signal {

		case syscall.SIGTERM:
			WARN.Println("Got SIGTERM. Program will terminate cleanly now.")
			os.Exit(143)
		case syscall.SIGINT:
			WARN.Println("Got SIGINT. Program will terminate cleanly now.")
			os.Exit(137)
		}
	}
}

// save start of the program time
func init_time() {
	STARTEDON = time.Now()
}

// return how long has the server been runing
func uptime(start time.Time) string {
	return time.Since(start).String()
}

// replace tags on DOCHTML
func init_html(srvaddr string) {

	srvaddr = strings.ToLower(srvaddr)

	if !strings.HasPrefix(srvaddr, "http://") {
		srvaddr = "http://" + srvaddr
	}

	if !strings.HasSuffix(srvaddr, "/") {
		srvaddr = srvaddr + "/"
	}

	DOCHTML = bytes.ReplaceAll(DOCHTML, []byte("$$srvaddr$$"), []byte(srvaddr))
	DOCHTML = bytes.ReplaceAll(DOCHTML, []byte("$$version$$"), []byte(VERSION))
}

// check the urls submited via command line are valid webhooks
func init_webhook(evtaddrs ArrayOfParams) {
	if len(evtaddrs) == 0 {
		return
	}

	for _, evtaddr := range evtaddrs {

		url, err := url.Parse(evtaddr)
		if err != nil {
			WARN.Printf("%s is not a valid url for the event server webhook. This eventserver won't be used", evtaddr)
			continue
		}

		_, err = net.LookupIP(url.Hostname())

		if err != nil {
			WARN.Printf("%s cannot be resolved to an ip. This eventserver won't be used.", url.Host)
			continue
		}

		INFO.Printf("%s will be used as eventserver webhook", evtaddr)
		EVTSERVER_WEBHOOKS = append(EVTSERVER_WEBHOOKS, evtaddr)

	}

}
