package http

import (
	"context"
	"crypto/tls"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"goscan/scanner"
	"goscan/utils"
)

func Root(ctx context.Context, target scanner.Target) (bool, error) {
	response, body, err := get(ctx, target, "/", 16*1024)
	if err != nil {
		return false, err
	}
	printBody(target, "/", response, body)
	return true, nil
}

func get(ctx context.Context, target scanner.Target, path string, limit int64) (*http.Response, string, error) {
	scheme := "http"
	if target.Port == 443 {
		scheme = "https"
	}
	url := fmt.Sprintf("%s://%s:%d%s", scheme, target.IP, target.Port, path)
	request, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
	if err != nil {
		return nil, "", err
	}
	request.Host = target.IP.String()
	request.Header.Set("User-Agent", "Mozilla/5.0")
	request.Header.Set("Connection", "close")

	transport := &http.Transport{
		TLSClientConfig:   &tls.Config{InsecureSkipVerify: true}, // Match the Python grabber.
		DisableKeepAlives: true,
	}
	defer transport.CloseIdleConnections()
	client := &http.Client{
		Timeout:   4 * time.Second,
		Transport: transport,
		CheckRedirect: func(_ *http.Request, _ []*http.Request) error {
			return http.ErrUseLastResponse
		},
	}

	response, err := client.Do(request)
	if err != nil {
		return nil, "", err
	}
	defer response.Body.Close()
	body, err := io.ReadAll(io.LimitReader(response.Body, limit))
	if err != nil {
		return nil, "", err
	}
	return response, strings.ToValidUTF8(string(body), "�"), nil
}

func printBody(target scanner.Target, path string, response *http.Response, body string) {
	utils.Sprint("%s:%d -> GET %s | %s\n%s", target.IP, target.Port, path, response.Status, body)
}
