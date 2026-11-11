#!/bin/bash
curl -X DELETE https://lobby.fujinet.online/server -H "Content-Type: application/json" -d '{"serverurl": "http://192.168.68.61:8080/?table=ai2"}'
curl -X DELETE https://lobby.fujinet.online/server -H "Content-Type: application/json" -d '{"serverurl": "http://192.168.68.61:8080/?table=ai4"}'
curl -X DELETE https://lobby.fujinet.online/server -H "Content-Type: application/json" -d '{"serverurl": "http://192.168.68.61:8080/?table=ai6"}'
curl -X DELETE https://lobby.fujinet.online/server -H "Content-Type: application/json" -d '{"serverurl": "http://192.168.68.61:8080/?table=basement"}'
curl -X DELETE https://lobby.fujinet.online/server -H "Content-Type: application/json" -d '{"serverurl": "http://192.168.68.61:8080/?table=den"}'

curl -X DELETE https://lobby.fujinet.online/server -H "Content-Type: application/json" -d '{"serverurl": "https://192.168.68.61/?table=ai2"}'
curl -X DELETE https://lobby.fujinet.online/server -H "Content-Type: application/json" -d '{"serverurl": "https://192.168.68.61/?table=ai4"}'
curl -X DELETE https://lobby.fujinet.online/server -H "Content-Type: application/json" -d '{"serverurl": "https://192.168.68.61/?table=ai6"}'
curl -X DELETE https://lobby.fujinet.online/server -H "Content-Type: application/json" -d '{"serverurl": "https://192.168.68.61/?table=basement"}'
curl -X DELETE https://lobby.fujinet.online/server -H "Content-Type: application/json" -d '{"serverurl": "https://192.168.68.61/?table=den"}'