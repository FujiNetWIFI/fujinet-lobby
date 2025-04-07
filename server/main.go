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
)

const (
	VERSION   = "5.5.1rc/multiple-web-hooks"
	STRINGVER = "fujinet persistent lobby  " + VERSION + "/" + runtime.GOOS + " (c) Roger Sen 2025"
)

//go:embed doc.html
var DOCHTML []byte

//go:embed servers.html
var SERVERS_HTML []byte

func main() {

	var srvaddr string
	var evtaddrs ArrayOfParams
	var help, version bool

	flag.StringVar(&srvaddr, "srvaddr", ":8080", "<address:port> for http server")
	flag.Var(&evtaddrs, "evtaddr", "<http> for event server webhook (multiple values accepted)")

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

	router := gin.Default()

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
	}
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

// check the url submited via command line is a valid webhook
func init_webhook(evtaddrs ArrayOfParams) {
	if len(evtaddrs) == 0 {
		return
	}

	for _, evtaddr := range evtaddrs {

		url, err := url.Parse(evtaddr)
		if err != nil {
			WARN.Printf("%s is not a valid url for the event server webhook. Eventserver won't be used", evtaddr)
			return
		}

		_, err = net.LookupIP(url.Host)

		if err != nil {
			WARN.Printf("%s cannot be resolved to an ip. Eventserver won't be used.", url.Host)
		}

		INFO.Printf("%s will be used as eventserver webhook", evtaddr)
		EVTSERVER_WEBHOOKS = append(EVTSERVER_WEBHOOKS, evtaddr)

	}

}
